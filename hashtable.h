#include <iostream>
// #include <bits/stdc++.h>


struct HNode{
    struct HNode *next = nullptr;
    uint64_t hashcode = 0; 
};

struct HTable{
    HNode **tab = nullptr;
    size_t current_size = 0; //size of hashtable
    size_t mask = 0; //understand mask 

};

struct HMap{
    HTable newer;
    HTable older;
    size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_clear(HMap *hmap);
size_t hm_size(HMap *hmap);

void hm_foreach(HMap *hmap,bool (*f)(HNode *, void *), void *arg);