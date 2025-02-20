#include "heap.h"

int heap_parent(size_t t)
    return (t + 1)/2 - 1;


int heap_left(size_t t)
    return t*2 + 1;


int heap_right(size_t t)
    return t*2 + 2;

//if the parent is greater, swap with the child it will be a loop/recursion
static void move_up(HeapElement *a, size_t pos)
{
    HeapElement t = a[pos];
    if(pos >0 && t.val < a[heap_parent(pos)].val)
    {

        a[pos] = a[parent_pos];
        //a[pos] points to parents, the reference index points to parents needs to be updated. 
        *a[pos].ref = pos; 
        a[parent_pos] = t;
        *a[parent_pos].ref = parent_pos;
    }
    else
    {
        return;
    }
    move_up(a, parent_pos);
}

static void move_down(HeapElement *a, size_t pos, size_t len)
{
    HeapElement t = a[pos];
    size_t l = heap_left(pos);
    size_t r = heap_right(pos);
    size_t min_pos = pos;
    if(l < len && a[l].val < a[pos].val)
    {
        min_pos = l;
    }
    if(r < len && a[r].val < a[min_pos].val)
    {
        min_pos = r;
    }
    if(min_pos != pos)
    {
        a[pos] = a[min_pos];
        //a[pos] points to parents, the reference index points to parents needs to be updated. 
        *a[pos].ref = pos; 
        a[min_pos] = t;
        *a[min_pos].ref = min_pos;
        move_down(a, min_pos, len);
    }

}

//*a points to the first element of the array
static void heap_update(HeapElement *a, size_t pos, size_t len)
{
    if(pos >0 && a[pos].val < a[heap_parent(pos)].val)
    {
        move_up(a, pos);
    }
    else
    {
        move_down(a, pos, len);
    }
}
//update and insert
static void heap_upsert(std::vector<HeapElement> &a, size_t pos, HeapElement t)
{   
    //update the element
    if(pos < a.size())
    {
        a[pos] = t;
    }
    else
    {
        pos = a.size();
        a.push_back(t);
    }
    heap_update(a.data(), pos, a.size());
}

static heap_delete(std::vector<HeapElement> &a, size_t pos)
{

    a[pos] = a.back();
    a.pop_back();

    //why check, just in case array is empty? 
    if(pos < a.size())
        heap_update(a.data(), pos, a.size());
    
}