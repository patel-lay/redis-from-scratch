#include <assert.h>
#include <stdlib.h>     // calloc(), free()
#include "hashtable.h"

const size_t k_rehashing_work = 128;   

const size_t k_max_load_factor = 8;
static void h_init(HTable *htable, size_t n)
{
    assert(n > 0 && ((n-1) & n)  == 0);
    htable->tab = (HNode **)calloc(n, sizeof(HNode *));
    htable->current_size = 0;
    htable->mask = n - 1;

}

static void h_insert(HTable *htab, HNode *node)
{
    size_t pos = node->hashcode & htab->mask;
    HNode *head = htab->tab[pos];
    node->next = head;
    htab->tab[pos] = node;
    htab->current_size++;
}

static HNode **h_lookup(HTable *htab, HNode *node, bool (*eq)(HNode *, HNode *))
{
    if (!htab->tab)
        return NULL;

    size_t pos = node->hashcode & htab->mask;
    HNode **from = &htab->tab[pos];
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) 
    {
        if(cur->hashcode == node->hashcode && eq(cur, node))
        {
          // std:: cout << cur->hashcode << "\n";
            return from;
        }
    }
    return NULL;
}

static HNode *h_detach(HTable *htable, HNode **from)
{   
    HNode *node = *from;
    *from = node->next;
    htable->current_size--;
    return node;

}

static void h_foreach(HTable *htable, bool (*f)(HNode *, void *), void *arg)
{
    if(!htable)
        return;
    for(size_t t = 0; htable->mask!=0 && t <= htable->mask; t++)
    {
        HNode *node = htable->tab[t];
        while(node!=NULL)
        {
            if(!f(node, arg))
                return ;
            node = node->next;
        }
    }
    return;
}

static void hm_trigger_rehashing(HMap *hmap) {
    assert(hmap->older.tab == NULL);
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

static void hm_help_rehashing(HMap *hmap) 
{
    size_t nwork = 0;
    while (nwork < k_rehashing_work && hmap->older.current_size > 0) {
        // find a non-empty slot
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from) {
            hmap->migrate_pos++;
            continue;   // empty slot
        }
        // move the first list item to the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.current_size == 0 && hmap->older.tab) {
        free(hmap->older.tab);
        hmap->older = HTable{};
    }

}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
     if (!from) {
        from = h_lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

void hm_insert(HMap *hmap, HNode *node)
{
    if(!hmap->newer.tab)
        h_init(&hmap->newer, 8);

    h_insert(&hmap->newer, node);

    if (!hmap->older.tab) {         // check whether we need to rehash
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.current_size >= shreshold) {
            hm_trigger_rehashing(hmap);
        }
    }
   hm_help_rehashing(hmap);

}


HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_rehashing(hmap);

    if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if (HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }
    return NULL;
}


void hm_clear(HMap *hmap)
{
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = HMap{};
}

size_t hm_size(HMap *hmap)
{
    return hmap->newer.current_size + hmap->older.current_size;
}

void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *arg)
{
    h_foreach(&hmap->newer, f, arg);
    h_foreach(&hmap->older, f, arg);
}