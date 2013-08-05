//
//  cache.h
//  cache
//
//  Created by Napat Luevisadpaibul on 8/5/13.
//  Copyright (c) 2013 Napat Luevisadpaibul. All rights reserved.
//

#include "csapp.h"

struct cache_node
{
    char *key;
    char *data;
    struct cache_node *next;
};


pthread_rwlock_t lock;

char *Get_data(char* key);
struct cache_node *Insert_atfront(char* key, char *data);
struct cache_node *Insert_node_atfront(struct cache_node *node);
struct cache_node *Remove_atTail();

void print_cache();

