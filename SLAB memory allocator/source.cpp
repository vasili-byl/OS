#include <iostream>
#include <vector>

const size_t PAGE_SIZE = 4096;

void* alloc_slab(int order) {
	size_t size = PAGE_SIZE * (1 << order);
	return malloc(size);
}

void free_slab(void* slab) {
	free(slab);
}

struct object_header;

struct SLAB {
	object_header* free_objects_head;
	size_t capacity;
	SLAB* next;
	SLAB* prev;
};

struct object_header {
	SLAB* owner;
	object_header* next;
};

struct cache {
	/* список пустых SLAB-ов для поддержки cache_shrink */
	SLAB* free_slabs_head;
	/* список частично занятых SLAB-ов */
	SLAB* partial_slabs_head;
	/* список заполненых SLAB-ов */
	SLAB* full_slabs_head;

	size_t object_size; /* размер аллоцируемого объекта */
	size_t adjusted_object_size;
	int slab_order; /* используемый размер SLAB-а */
	size_t slab_objects; /* количество объектов в одном SLAB-е */
};

void cache_setup(struct cache* cache, size_t object_size)
{
	cache->free_slabs_head = NULL;
	cache->partial_slabs_head = NULL;
	cache->full_slabs_head = NULL;
	cache->object_size = object_size;
	cache->adjusted_object_size = object_size + sizeof(object_header);
	cache->slab_order = 5;
	cache->slab_objects = ((PAGE_SIZE * (1 << cache->slab_order)) - sizeof(SLAB)) / cache->adjusted_object_size;
}

void free_slabs_in_list(SLAB* head) {
	SLAB* current = head;
	while (current != NULL) {
		SLAB* next = current->next;
		free_slab(current);
		current = next;
	}
}

void cache_release(struct cache* cache)
{
	free_slabs_in_list(cache->free_slabs_head);
	cache->free_slabs_head = NULL;
	free_slabs_in_list(cache->partial_slabs_head);
	cache->partial_slabs_head = NULL;
	free_slabs_in_list(cache->full_slabs_head);
	cache->full_slabs_head = NULL;
}

void remove_slab(cache* cache, SLAB* slab) {
	SLAB* prev = slab->prev;
	SLAB* next = slab->next;
	slab->next = NULL;
	slab->prev = NULL;
	if ((prev != NULL) && (next != NULL)) {
		prev->next = next;
		next->prev = prev;
	}
	else if (prev != NULL) {
		prev->next = NULL;
	}
	else if (next != NULL) {
		next->prev = NULL;
	}

	if (cache->free_slabs_head == slab) {
		cache->free_slabs_head = next;
	}
	else if (cache->partial_slabs_head == slab) {
		cache->partial_slabs_head = next;
	}
	else if (cache->full_slabs_head == slab) {
		cache->full_slabs_head = next;
	}
}

void* cache_alloc(struct cache* cache)
{
	SLAB* slab = NULL;
	if (cache->partial_slabs_head != NULL) {
		slab = cache->partial_slabs_head;
		remove_slab(cache, slab);
	} else if (cache->free_slabs_head != NULL) {
		slab = cache->free_slabs_head;
		remove_slab(cache, slab);
	}
	else {
		slab = (SLAB*)alloc_slab(cache->slab_order);
		slab->capacity = cache->slab_objects;
		slab->next = NULL;
		slab->prev = NULL;
		char* ptr = (char*)slab + sizeof(SLAB);
		for (int i = 1; i < cache->slab_objects; ++i) {
			object_header* header = (object_header*)(ptr + i * cache->adjusted_object_size);
			object_header* previous = (object_header*)(ptr + (i - 1) * cache->adjusted_object_size);
			previous->next = header;
			header->next = NULL;
			header->owner = NULL;
		}
		object_header* header = (object_header*)ptr;
		header->owner = NULL;
		slab->free_objects_head = header;
	}

	object_header* object = slab->free_objects_head;
	slab->capacity -= 1;
	slab->free_objects_head = object->next;
	object->next = NULL;
	object->owner = slab;

	if (slab->capacity == 0) {
		slab->next = cache->full_slabs_head;
		if (cache->full_slabs_head != NULL) {
			cache->full_slabs_head->prev = slab;
		}
		cache->full_slabs_head = slab;
	}
	else {
		slab->next = cache->partial_slabs_head;
		if (cache->partial_slabs_head != NULL) {
			cache->partial_slabs_head->prev = slab;
		}
		cache->partial_slabs_head = slab;
	}

	return (char*)object + sizeof(object_header);
}

void cache_free(struct cache* cache, void* ptr)
{
	object_header* object = (object_header*)((char*)ptr - sizeof(object_header));
	SLAB* slab = object->owner;
	remove_slab(cache, slab);
	object->owner = NULL;
	slab->capacity += 1;
	object->next = slab->free_objects_head;
	slab->free_objects_head = object;

	if (slab->capacity == cache->slab_objects) {
		slab->next = cache->free_slabs_head;
		if (cache->free_slabs_head != NULL) {
			cache->free_slabs_head->prev = slab;
		}
		cache->free_slabs_head = slab;
	}
	else {
		slab->next = cache->partial_slabs_head;
		if (cache->partial_slabs_head != NULL) {
			cache->partial_slabs_head->prev = slab;
		}
		cache->partial_slabs_head = slab;
	}
}

void cache_shrink(struct cache* cache)
{
	free_slabs_in_list(cache->free_slabs_head);
	cache->free_slabs_head = NULL;
}

int main() {
	cache cache_model;
	cache* cache_ptr = &cache_model;
	cache_setup(cache_ptr, 1000);
	std::vector<char*> ptrs;
	for (int i = 0; i < 10000; ++i) {
		std::cout << i << std::endl;
		if ((ptrs.size() > 0) && (rand() % 3 == 1)) {
			size_t index = rand() % ptrs.size();
			cache_free(cache_ptr, ptrs[index]);
			ptrs.erase(ptrs.begin() + index);
		}
		else {
			ptrs.push_back((char*)cache_alloc(cache_ptr));
		}
	}
	cache_release(cache_ptr);
 	return 0;
}