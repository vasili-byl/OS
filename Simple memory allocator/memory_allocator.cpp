#include <iostream>
#include <vector>

struct block_border_t {
	size_t size;
	bool is_free;
};

struct header_t {
	block_border_t border;
	header_t* next_block;
	header_t* prev_block;
};

char* BUFFER_BEGIN = NULL;
char* BUFFER_END = NULL;
header_t* BLOCKS_LIST_HEAD = NULL;
size_t MINIMUM_FREE_BLOCK_SIZE = 10;

void mysetup(void* buf, std::size_t size)
{
	// Remember memory bounds
	BUFFER_BEGIN = (char*)buf;
	BUFFER_END = BUFFER_BEGIN + size;

	size_t effective_size = size - sizeof(header_t) - sizeof(block_border_t);
	
	BLOCKS_LIST_HEAD = (header_t*)buf;
	BLOCKS_LIST_HEAD->border.size = effective_size;
	BLOCKS_LIST_HEAD->border.is_free = true;
	BLOCKS_LIST_HEAD->next_block = NULL;
	BLOCKS_LIST_HEAD->prev_block = NULL;

	block_border_t* footer = (block_border_t*)(BUFFER_BEGIN + size - sizeof(block_border_t));
	footer->size = effective_size;
	footer->is_free = true;
}

size_t get_busy_block_size(size_t free_block_effective_size) {
	return free_block_effective_size + sizeof(header_t) - sizeof(block_border_t);
}

void block_list_remove_block(header_t* block) {
	header_t* prev = block->prev_block;
	header_t* next = block->next_block;
	if ((next != NULL) && (prev != NULL)) {
		prev->next_block = next;
		next->prev_block = prev;
	}
	else if (next != NULL) {
		next->prev_block = NULL;
	}
	else if (prev != NULL) {
		prev->next_block = NULL;
	}

	if (BLOCKS_LIST_HEAD == block) {
		BLOCKS_LIST_HEAD = next;
	}
}

void* myalloc(std::size_t size)
{
	// Find block with effective_size >= size
	header_t* appropriate_block = BLOCKS_LIST_HEAD;
	while ((appropriate_block != NULL) && (get_busy_block_size(appropriate_block->border.size) < size)) {
		appropriate_block = appropriate_block->next_block;
	}

	if (appropriate_block == NULL) {
		return NULL;
	}
	
	// Change structure of free blocks or just found block properties
	size_t free_block_size = appropriate_block->border.size;
	char* raw_memory = NULL;
	size_t raw_memory_size = 0;
	//if (get_busy_block_size(free_block_size) - size < MINIMUM_FREE_BLOCK_SIZE) {
	if (free_block_size + sizeof(header_t) - size - sizeof(block_border_t) < sizeof(header_t) + sizeof(block_border_t) + MINIMUM_FREE_BLOCK_SIZE) {
		block_list_remove_block(appropriate_block);
		raw_memory_size = free_block_size + sizeof(header_t) + sizeof(block_border_t);
		raw_memory = (char*)appropriate_block;
	} else {
		raw_memory_size = size + 2 * sizeof(block_border_t);
		raw_memory = (char*)appropriate_block + sizeof(header_t) + appropriate_block->border.size + sizeof(block_border_t) - raw_memory_size;
		appropriate_block->border.size = appropriate_block->border.size - size - 2 * sizeof(block_border_t);
		block_border_t* footer = (block_border_t*)(raw_memory - sizeof(block_border_t));
		footer->is_free = true;
		footer->size = appropriate_block->border.size;
	}

	// Make header and footer for new busy block
	block_border_t* header = (block_border_t*)raw_memory;
	header->size = raw_memory_size - 2 * sizeof(block_border_t);
	header->is_free = false;
	block_border_t* footer = (block_border_t*)(raw_memory + raw_memory_size - sizeof(block_border_t));
	footer->size = header->size;
	footer->is_free = false;
	return raw_memory + sizeof(block_border_t);
}

void myfree(void* p)
{
	block_border_t* current_header = (block_border_t*)((char*)p - sizeof(block_border_t));
	char* raw_memory = (char*)current_header;
	size_t raw_memory_size = current_header->size + 2 * sizeof(block_border_t);
	// Connect with left block if it is free
	if ((char*)current_header > BUFFER_BEGIN) {
		block_border_t* prev_footer = (block_border_t*)((char*)current_header - sizeof(block_border_t));
		if (prev_footer->is_free) {
			header_t* prev_header = (header_t*)((char*)prev_footer - prev_footer->size - sizeof(header_t));
			block_list_remove_block(prev_header);
			raw_memory = (char*)prev_header;
			raw_memory_size += prev_header->border.size + sizeof(header_t) + sizeof(block_border_t);
		}
	}

	// Connect with right block if it is free
	if ((char*)current_header + current_header->size + 2 * sizeof(block_border_t) < BUFFER_END) {
		block_border_t* header_next = (block_border_t*)((char*)current_header + current_header->size + 2 * sizeof(block_border_t));
		if (header_next->is_free) {
			block_list_remove_block((header_t*)header_next);
			raw_memory_size += header_next->size + sizeof(header_t) + sizeof(block_border_t);
		}
	}

	// Create header and footer for new free block
	header_t* header = (header_t*)raw_memory;
	header->border.size = raw_memory_size - sizeof(header_t) - sizeof(block_border_t);
	header->border.is_free = true;
	block_border_t* footer = (block_border_t*)(raw_memory + raw_memory_size - sizeof(block_border_t));
	footer->size = header->border.size;
	footer->is_free = true;

	// Add block to the list
	header->prev_block = NULL;
	if (BLOCKS_LIST_HEAD == NULL) {
		header->next_block = NULL;
	} else {
		BLOCKS_LIST_HEAD->prev_block = header;
		header->next_block = BLOCKS_LIST_HEAD;
	}
	BLOCKS_LIST_HEAD = header;
}

void check(void* p)
{
	if (p == NULL) {
		std::cout << "NULL pointer" << std::endl;
	} else {
		std::cout << "Pointer is not NULL" << std::endl;
	}
}

void log_memory_state() {
	std::cout << "=====LOG=====" << std::endl;
	char* current_addr = BUFFER_BEGIN;
	while (current_addr < BUFFER_END)
	{
		block_border_t* border = (block_border_t*)current_addr;
		std::cout << border->size << "(" << (border->is_free ? "free" : "busy") << ") ";
		current_addr += border->size + sizeof(block_border_t);
		if (border->is_free) {
			current_addr += sizeof(header_t);
		} else {
			current_addr += sizeof(block_border_t);
		}
	}
	std::cout << std::endl;
}

int main() {
	int N = 150;
	char* data = (char*)malloc(sizeof(char) * N);
	mysetup(data, N);

	// Random test
	std::vector<char*> pointers;
	for (int i = 0; i < 1000; ++i) {
		std::cout << i << std::endl;
		if (i == 10) {
			std::cout << "Found";
		}
		if ((pointers.size() == 0) || (rand() % 3 != 0)) {
			size_t size = rand() % 100;//(rand() % 100) + MINIMUM_FREE_BLOCK_SIZE;
			char* p = (char*)myalloc(size);
			if (p != NULL) {
				pointers.push_back(p);
			}
		} else {
			int index = rand() % pointers.size();
			myfree(pointers[index]);
			pointers.erase(pointers.begin() + index);
		}
	}

	free(data);
	return 0;
}