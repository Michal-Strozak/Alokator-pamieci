#include "heap.h"
#include "custom_unistd.h"
#include <string.h>
#include "tested_declarations.h"
#include "rdebug.h"

#define FENCE 4
#define PAGE 4096

struct memory_manager_t
{
    void *memory_start;
    size_t memory_size;
    size_t used_memory;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t {
    size_t control_sum;
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    size_t free;
};

struct memory_manager_t memory_manager;

void set_fences(void *address, size_t size)
{
    if(address==NULL || size<1)
    {
        return;
    }
    address=(char*)address;
    memset(address,'#',FENCE);
    address=(char*)address+FENCE;
    address=(char*)address+size;
    memset(address,'#',FENCE);
}

int heap_setup(void)
{
    void *start=custom_sbrk(PAGE);
    if(start==(void*)-1)
    {
        return -1;
    }
    memory_manager.memory_start=start;
    memory_manager.memory_size=PAGE;
    memory_manager.used_memory=0;
    memory_manager.first_memory_chunk=NULL;
    return 0;
}

void heap_clean(void)
{
    custom_sbrk((intptr_t)memory_manager.memory_size*(-1));
    memory_manager.memory_size=0;
    memory_manager.used_memory=0;
    memory_manager.first_memory_chunk=NULL;
}

int heap_increase(size_t size)
{
    size_t pages=size/PAGE;
    pages++;
    void *new=custom_sbrk((intptr_t)pages*PAGE);
    if(new==(void*)-1)
    {
        return -1;
    }
    memory_manager.memory_size+=pages*PAGE;
    return 0;
}

void calculate_used_memory(void)
{
    memory_manager.used_memory=0;
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        if(temp->free==0)
        {
            memory_manager.used_memory+=sizeof(struct memory_chunk_t)+(2*FENCE)+temp->size;
        }
        temp=temp->next;
    }
}

void calculate_control_sum()
{
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        temp->control_sum=(size_t)temp->prev+(size_t)temp->next+temp->size+(size_t)temp->free;
        temp=temp->next;
    }
}

void* heap_malloc(size_t size)
{
    if(heap_validate()!=0 || size<1)
    {
        return NULL;
    }
    size_t memory_needed=size+sizeof(struct memory_chunk_t)+(2*FENCE);
    if(memory_needed>(memory_manager.memory_size-memory_manager.used_memory))
    {
        int add=heap_increase(memory_needed);
        if(add!=0)
        {
            return NULL;
        }
    }
    // pamięć całkowicie pusta
    if(memory_manager.first_memory_chunk==NULL)
    {
        memory_manager.first_memory_chunk=(struct memory_chunk_t*)memory_manager.memory_start;
        memory_manager.first_memory_chunk->size=size;
        memory_manager.first_memory_chunk->prev=NULL;
        memory_manager.first_memory_chunk->next=NULL;
        memory_manager.first_memory_chunk->free=0;
        memory_manager.used_memory+=memory_needed;
        calculate_control_sum();
        set_fences((char*)memory_manager.first_memory_chunk+sizeof(struct memory_chunk_t),memory_manager.first_memory_chunk->size);
        return (void*)((char*)memory_manager.first_memory_chunk+sizeof(struct memory_chunk_t)+FENCE);
    }
    struct memory_chunk_t *current_block=memory_manager.first_memory_chunk;
    while(current_block!=NULL)
    {
        if(current_block->free==1 && current_block->size>=size+(2*FENCE))
        {
            // znaleziono wolny blok pamięci
            current_block->free=0;
            current_block->size=size;
            memory_manager.used_memory+=memory_needed;
            calculate_control_sum();
            set_fences((char*)current_block+sizeof(struct memory_chunk_t),current_block->size);
            return (void*)((char*)current_block+sizeof(struct memory_chunk_t)+FENCE);
        }
        // szukanie pustych obszarów pamięci
        if(current_block->next!=NULL)
        {
            char *start=NULL;
            if(current_block->free==0)
            {
                start=(char*)current_block+sizeof(struct memory_chunk_t)+current_block->size+(2*FENCE);
            }
            else
            {
                start=(char*)current_block+sizeof(struct memory_chunk_t)+current_block->size;
            }
            char *stop=(char*)current_block->next;
            size_t free_memory=stop-start;
            if(free_memory>=size+sizeof(struct memory_chunk_t)+(2*FENCE))
            {
                struct memory_chunk_t *new=(struct memory_chunk_t*)((char*)current_block+sizeof(struct memory_chunk_t)+current_block->size+(2*FENCE));
                new->size=size;
                new->free=0;
                new->prev=current_block;
                new->next=current_block->next;
                current_block->next->prev=new;
                current_block->next=new;
                memory_manager.used_memory+=memory_needed;
                calculate_control_sum();
                set_fences((char*)new+sizeof(struct memory_chunk_t),new->size);
                return (void*)((char*)new+sizeof(struct memory_chunk_t)+FENCE);
            }
        }
        current_block=current_block->next;
    }
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp->next!=NULL)
    {
        temp=temp->next;
    }
    char *start=(char*)memory_manager.memory_start;
    char *stop=(char*)temp+sizeof(struct memory_chunk_t)+temp->size+(2*FENCE);
    size_t used_memory=stop-start;
    size_t free_memory=memory_manager.memory_size-used_memory;
    if(free_memory>=size+sizeof(struct memory_chunk_t)+(2*FENCE))
    {
        // dodawanie nowego bloku na koniec
        struct memory_chunk_t *new=(struct memory_chunk_t*)((char*)temp+sizeof(struct memory_chunk_t)+temp->size+(2*FENCE));
        temp->next=new;
        new->size=size;
        new->free=0;
        new->prev=temp;
        new->next=NULL;
        memory_manager.used_memory+=memory_needed;
        calculate_control_sum();
        set_fences((char*)new+sizeof(struct memory_chunk_t),new->size);
        return (void*)((char*)new+sizeof(struct memory_chunk_t)+FENCE);
    }
    return NULL;
}

void* heap_calloc(size_t number, size_t size)
{
    if(number<1 || size<1)
    {
        return NULL;
    }
    void *memory=heap_malloc(number*size);
    if(memory==NULL)
    {
        return NULL;
    }
    char *wsk=memory;
    for(size_t i=0;i<number*size;i++)
    {
        *(wsk+i)=0;
    }
    return memory;
}

void* heap_realloc(void* memblock, size_t count)
{
    if(heap_validate()!=0)
    {
        return NULL;
    }
    if(memblock!=NULL && count==0)
    {
        heap_free(memblock);
        return NULL;
    }
    if(memblock==NULL)
    {
        return heap_malloc(count);
    }
    int find=0;
    size_t memblock_size=0;
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        if((char*)memblock==(char*)temp+sizeof(struct memory_chunk_t)+FENCE)
        {
            memblock_size=temp->size;
            find++;
            break;
        }
        temp=temp->next;
    }
    if(find==0)
    {
        return NULL;
    }
    if(count==memblock_size)
    {
        return memblock;
    }
    else if(count<memblock_size)
    {
        temp->size=count;
        calculate_control_sum();
        calculate_used_memory();
        set_fences((char*)temp+sizeof(struct memory_chunk_t),temp->size);
        return memblock;
    }
    else if(count>memblock_size)
    {
        if(temp->next!=NULL)
        {
            size_t extra_size=(char*)temp->next-(char*)temp;
            extra_size=extra_size-sizeof(struct memory_chunk_t)-(2*FENCE);
            if(extra_size>=count)
            {
                temp->size=count;
                calculate_control_sum();
                calculate_used_memory();
                set_fences((char*)temp+sizeof(struct memory_chunk_t),temp->size);
                return memblock;
            }
        }
        if(heap_increase(count-memblock_size)!=0)
        {
            return NULL;
        }
        heap_free(memblock);
        char *new_memory=heap_malloc(count);
        if(new_memory==NULL)
        {
            return NULL;
        }
        for(size_t i=0;i<memblock_size;i++)
        {
            *(new_memory+i)=*((char*)memblock+i);
        }
        calculate_control_sum();
        calculate_used_memory();
        return new_memory;
    }
    return NULL;
}

void heap_free(void* memblock)
{
    if(memblock==NULL || heap_validate()!=0 || get_pointer_type(memblock)!=pointer_valid)
    {
        return;
    }
    struct memory_chunk_t *current_block=(struct memory_chunk_t*)((char*)memblock-sizeof(struct memory_chunk_t)-FENCE);
    // sprawdzanie poprawności przekazanego wskaźnika
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    int exist=0;
    while(temp!=NULL)
    {
        if(temp==current_block)
        {
            exist++;
            break;
        }
        temp=temp->next;
    }
    if(exist==0)
    {
        return;
    }
    current_block->free=1;
    // szukanie wolnej pamięci po zwolnionym bloku
    if(current_block->next!=NULL)
    {
        char *start=(char*)current_block+sizeof(struct memory_chunk_t)+current_block->size+(2*FENCE);
        char *stop=(char*)current_block->next;
        size_t extra_memory=stop-start;
        current_block->size=current_block->size+extra_memory;
    }
    current_block->size+=2*FENCE;
    // łączenie z następnym wolnym blokiem
    if(current_block->next!=NULL && current_block->next->free==1)
    {
        current_block->size=current_block->size+sizeof(struct memory_chunk_t)+current_block->next->size;
        current_block->next=current_block->next->next;
        if(current_block->next!=NULL)
        {
            current_block->next->prev=current_block;
        }
    }
    // łączenie z poprzednim wolnym blokiem
    if(current_block->prev!=NULL && current_block->prev->free==1)
    {
        current_block=current_block->prev;
        current_block->size=current_block->size+sizeof(struct memory_chunk_t)+current_block->next->size;
        current_block->next=current_block->next->next;
        if(current_block->next!=NULL)
        {
            current_block->next->prev=current_block;
        }
    }
    // sprawdzanie czy blok jest ostatnim
    if(current_block->next==NULL && current_block->prev!=NULL)
    {
        current_block->prev->next=NULL;
    }
    // sprawdzanie czy blok jest jedynym
    if(current_block->next==NULL && current_block->prev==NULL)
    {
        memory_manager.first_memory_chunk=NULL;
    }
    if(memory_manager.first_memory_chunk!=NULL)
    {
        set_fences((char*)current_block+sizeof(struct memory_chunk_t),current_block->size-(2*FENCE));
    }
    calculate_used_memory();
    calculate_control_sum();
}

size_t heap_get_largest_used_block_size(void)
{
    if(heap_validate()!=0)
    {
        return 0;
    }
    size_t max=0;
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        if(temp->free==0 && temp->size>max)
        {
            max=temp->size;
        }
        temp=temp->next;
    }
    return max;
}

enum pointer_type_t get_pointer_type(const void* const pointer)
{
    if(pointer==NULL)
    {
        return pointer_null;
    }
    if(heap_validate()!=0)
    {
        return pointer_heap_corrupted;
    }
    if(pointer<memory_manager.memory_start)
    {
        return pointer_unallocated;
    }
    int find=0;
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        if((char*)pointer>=(char*)temp && (char*)pointer<(char*)temp+sizeof(struct memory_chunk_t)+temp->size+(2*FENCE))
        {
            find++;
            break;
        }
        temp=temp->next;
    }
    if(find!=0)
    {
        if((char*)pointer<(char*)temp+sizeof(struct memory_chunk_t))
        {
            return pointer_control_block;
        }
        else if(temp->free==1)
        {
            return pointer_unallocated;
        }
        else if((char*)pointer<(char*)temp+sizeof(struct memory_chunk_t)+FENCE)
        {
            return pointer_inside_fences;
        }
        else if((char*)pointer==(char*)temp+sizeof(struct memory_chunk_t)+FENCE)
        {
            return pointer_valid;
        }
        else if((char*)pointer<(char*)temp+sizeof(struct memory_chunk_t)+FENCE+temp->size)
        {
            return pointer_inside_data_block;
        }
        else if((char*)pointer<(char*)temp+sizeof(struct memory_chunk_t)+2*FENCE+temp->size)
        {
            return pointer_inside_fences;
        }
    }
    return pointer_unallocated;
}

int heap_validate(void)
{
    if(memory_manager.memory_size==0 || memory_manager.memory_start==NULL)
    {
        return 2;
    }
    struct memory_chunk_t *temp=memory_manager.first_memory_chunk;
    while(temp!=NULL)
    {
        if(((size_t)temp->prev+(size_t)temp->next+temp->size+(size_t)temp->free)!=temp->control_sum)
        {
            return 3;
        }
        char *first_fence=(char*)temp+sizeof(struct memory_chunk_t);
        char *second_fence=NULL;
        if(temp->free==0)
        {
            second_fence=(char*)first_fence+FENCE+temp->size;
        }
        else
        {
            second_fence=(char*)first_fence+FENCE+(temp->size-(2*FENCE));
        }
        for(int i=0;i<FENCE;i++)
        {
            if(*(first_fence+i)!='#' || *(second_fence+i)!='#')
            {
                return 1;
            }
        }
        temp=temp->next;
    }
    return 0;
}
