//
//  cache.c
//  cache
//
//  Created by Napat Luevisadpaibul on 8/5/13.
//  Copyright (c) 2013 Napat Luevisadpaibul. All rights reserved.
//

#include "cache.h"
//#include <pthread.h>
static unsigned long MAX_SIZE = 1049000;
static unsigned long current_size = 0;

struct cache_node *head = NULL;

struct cache_node *Insert_atfront(char *key,char *data)
{
    struct cache_node *node;
    if(key==NULL || data == NULL)
    {
        printf("Error Inser_atfront: blank key or data\n");
        return NULL;
    }
    pthread_rwlock_wrlock(&lock);
    printf("\n\nbegin writer Lock at Insert Node Malloc\n");
    {
        node = Malloc(sizeof(struct cache_node));
    
        node->next = NULL;
        node->key = key;
        node->data = data;
    }
    pthread_rwlock_unlock(&lock);
    printf("\n\nend writer Lock at Insert Node Malloc\n");
    return Insert_node_atfront(node);
    
    
}

struct cache_node *Insert_node_atfront(struct cache_node *node)
{
    if(node == NULL)
    {
        printf("Error Inser_atfront: Null node\n");
        return NULL;
    }
    pthread_rwlock_wrlock(&lock);
    printf("\n\nbegin writer Lock at Insert Node\n");
    if(head == NULL) //empty list
    {
        current_size += strlen(node->data);
        node->next = NULL;
        head = node;
       
        pthread_rwlock_unlock(&lock);
        //print_cache();
        printf("\n\nend writer Lock at Insert Node Head\n");
        return node;
    }
    
   
    unsigned long size;
    while( (size=current_size + strlen(node->data)) > MAX_SIZE)
    {
        //printf("cache full: current size = %ld, node size = %ld\n\n",current_size,strlen(node->data));
        Remove_atTail();
        print_cache();
    }
    
    
    node->next = head;
    head = node;
    
    current_size += strlen(node->data);
    
    pthread_rwlock_unlock(&lock);
     printf("\n\nend writer Lock at Insert Node\n");
    
    print_cache();
    return node;
}

struct cache_node *Remove_atTail()
{

    
    struct cache_node *current = head;
    struct cache_node * prev = NULL;
    //struct cache_node * tail = NULL;
    if(head == NULL)
    {
        return NULL;
    }
    while (current->next != NULL) {
        prev = current;
        current = current->next;
    }
    current_size -= strlen(current->data);
    
    if(prev != NULL)
    {
        prev->next = NULL; //remove current node which is the last node on list
    }
    else //only head node so we remove head
    {
        head = NULL;
    }
    
    print_cache();
    return prev; //return tail
}

char* Get_data(char* key){
    
    if(key == NULL)
    {
        printf("invalid key\n");
        return NULL;
    }
    
    pthread_rwlock_rdlock (&lock);
    
    printf("\n\nbegin Reader Lock at Get Data\n");
    
    struct cache_node *current = head;
    struct cache_node * result = NULL;
    struct cache_node * prev = NULL;
    
    //searching should use reader block
    print_cache();
    while(current != NULL)
    {
        printf("\ncurr= %s, key=%s \n",current->key,key);
        if(current->key != NULL && strcasecmp (key, current->key) == 0 )
        {
            printf("\n\n Match !!!!\n");
            result = current;
            break;
        }
        else
        {
            printf("\n\n Not Match");
        }
        prev = current;
        current = current->next;
    }
    
    pthread_rwlock_unlock(&lock);
    printf("\n\nEnd Reader Lock at Get Data\n");
    if(result == NULL)
    {
        return NULL;
    }
    
    //this area modify cache, should use writer block
    
    pthread_rwlock_wrlock(&lock);
    printf("\n\nbegin Writer Lock at Get Data\n");
    //delete this node from list
    /*if(result == head) //head of the list
    {
        head = result->next;
    }*/
    if(result != head)
    {
        printf("\n\nResult is not Head\n");
        prev->next = result->next;
        current_size -= strlen(result->data);
        //move node to front
        pthread_rwlock_unlock(&lock);
        printf("\n\nEnd Writer Lock at Get Data\n");
        
        
        Insert_node_atfront(result);
    
        
    }
    else{
        printf("\n\nResult is Head\n");
        pthread_rwlock_unlock(&lock);
        printf("\n\nEnd Writer Lock at Get Data\n");
        
    }
    print_cache();
    printf("\n\result: %p\n",result);
    printf("\n\ndata: %s\n",result->data);
    printf("\n\ndata lenght: %ld\n",strlen(result->data));
    return result->data;
}


void print_cache(){

    
    struct cache_node *current = head;
    printf("total size = %ld\n",current_size);
    while (current != NULL) {
        
       // printf("node%p: key =%s, data=%s, size=%zd, next=%p\n",current,current->key,
       //        current->data,strlen(current->data),current->next);
        printf(" node=%p: ",current);
        if(current->key == NULL)
        {
            printf(" KEY NULL");
            
        }
        else
        {
            printf(" KEY= %s",current->key);
        }
        if(current->data == NULL)
        {
            printf(" DATA NULL");
            
        }
        else
        {
            printf(" DATA= %ld",strlen(current->data));
        }

        printf(" next=%p: ",current->next);
            
        current = current->next;
    }
    printf("\n\n");
    
}