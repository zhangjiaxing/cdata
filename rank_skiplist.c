/*
一个支持排名和多重key的skiplist
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


#define SKIPLIST_MAXLEVEL 32 /* Should be enough for 2^64 elements */
#define SKIPLIST_P 0.25      /* Skiplist P = 1/4 */


typedef enum element_type {
    TINT32,
    TUINT32,
    TINT64,
    TUINT64,
    TFLOAT,
    TDOUBLE,
    TSTR,
    TPTR,
    TUNKNOW
} element_type_t;


char* const element_typename_list[] = {
    "i32", "u32", "i64", "u64", "float", "double", "string", "pointer", "unknow type"
};


typedef union element {
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f;
    double d;
    char *s;
    void *p;
} element_t;



#define ELEMENT_TYPEID(x) _Generic((x), \
                            int32_t: TINT32, uint32_t: TUINT32, \
                            int64_t: TINT64, uint64_t: TUINT64, \
                            float: TFLOAT, double: TDOUBLE, \
                            char*: TSTR, void *: TPTR, \
                            default: TUNKNOW )

#define ELEMENT_TYPENAME(x) (element_typename_list[ELEMENT_TYPEID(x)])


// #define skip_list_foreach(node, l) \
//         for ((node) = (l)->header->level[0].forward; (node)!=(l)->header; (node)=(node)->level[0].forward)


// #define skip_list_foreach_safe(node, l) \
//         (node) = (l)->header->level[0].forward; \
//         for (skip_node_t *tMp__=(node)->level[0].forward; (node)!=(l)->header; (node)=tMp__, tMp__=(node)->level[0].forward)


// #define skip_list_foreach_reverse(node, l) \
//         for ((node) = (l)->header->backward; node!=(l)->header; (node)=(node)->backward)


// #define skip_list_foreach_reverse_safe(node, l) \
//         (node) = (l)->header->backward; \
//         for (skip_node_t *tMp__=(node)->backward; (node)!=(l)->header; (node)=tMp__, tMp__=(node)->backward)

#define DEF_ELEMENT_PRINT(TYPE, FIELD, PRINTF_FMT) \
void print_element_##FIELD(element_t ele){ \
    printf(PRINTF_FMT "-", ele.FIELD); \
}


DEF_ELEMENT_PRINT(uint32_t, u32, "%lu")
DEF_ELEMENT_PRINT(int32_t, i32, "%d")
DEF_ELEMENT_PRINT(int64_t, i64, "%lld")
DEF_ELEMENT_PRINT(uint64_t, u64, "%llu")
DEF_ELEMENT_PRINT(float, f, "%f")
DEF_ELEMENT_PRINT(double, d, "%f")
DEF_ELEMENT_PRINT(char*, s, "%s")
DEF_ELEMENT_PRINT(void*, p, "%p")


typedef struct skip_node skip_node_t;
typedef struct skip_list skip_list_t;


typedef skip_node_t* (*insert_func_t)(skip_list_t *l, element_t key, element_t value);
typedef element_t (*remove_func_t)(skip_list_t *l, element_t key);
typedef skip_node_t* (*find_func_t)(skip_list_t *l, element_t key);
typedef void (*print_func_t)(skip_list_t *l);


struct skip_node {
    element_t key; //skiplist按照key的大小顺序存放, 当key一样时候, 按照skip_node的内存中地址顺序存放, 这样给定skip_node指针时候, 可以删除很快.
    element_t value;

    skip_node_t *backward;
    struct skiplist_level {
        skip_node_t *forward;
        //span在节点中存放到forward节点的距离,header节点中span存放到第一个节点中的距离, level[0]最后一个节点的span应该为0
        //这样insert时候, 只需要计算backward节点和当前节点的span, 不需要计算forward节点的span. 这样可以不需要判断forward节点是否是NULL/header.
        unsigned long span; 
    }level[];
};


struct skip_list {
    unsigned long length;
    int level;
    skip_node_t *header; //循环列表, 方便逆序遍历

    element_type_t key_type;
    element_type_t value_type;
    
    insert_func_t insert_func;
    remove_func_t remove_func;
    find_func_t find_func;
    print_func_t print_func;
};

// skip_node_t *skip_node_create(int level, int key, int value){
//     skip_node_t *node = malloc(sizeof(*node) + level*(sizeof(struct skiplist_level)));
//     node->key = key;
//     node->value = value;
//     return node;
// }

#define DEF_SKIP_NODE_CREATE(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_node_t *skip_node_create_ ## KEY_FIELD ## _ ## VALUE_FIELD(int level, KEY_TYPE key, VALUE_TYPE value){ \
    skip_node_t *node = malloc(sizeof(*node) + level*(sizeof(struct skiplist_level))); \
    node->key.KEY_FIELD = key; \
    node->value.VALUE_FIELD = value; \
    return node; \
} \


void skip_node_destroy(skip_node_t *node){
    free(node);
}

#define DEF_SKIP_LIST_PRINT(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
void skip_list_print_ ## KEY_FIELD ## _ ## VALUE_FIELD(skip_list_t *l){ \
    printf("\nskiplist: count %d\n", l->length); \
    for(int i=l->level-1; i>=0; i--){ \
        printf("level %d: ", i); \
        for(skip_node_t *cur=l->header->level[i].forward; cur!=l->header; cur=cur->level[i].forward){ \
            print_element_##KEY_FIELD(cur->key); \
        } \
        printf("NULL\n"); \
    } \
} \

// skip_list_t* skip_list_create(){
//     skip_list_t *slist = malloc(sizeof(*slist));
//     slist->level = 1;
//     slist->length = 0;

//     skip_node_t *header = skip_node_create(SKIPLIST_MAXLEVEL, INT_MIN, INT_MIN); //方便debug,容易发现是头节点
//     header->backward = header;
//     for(int i=0; i<SKIPLIST_MAXLEVEL; i++){
//         header->level[i].forward = header; // 使用循环链表, 方便实现 skip_list_for_each_safe
//         header->level[i].span = 0;
//     }

//     slist->header = header;
//     slist->tail = header;  //可以不要, 等价于header->level[0].backward, 只是为了方便
//     return slist;
// }

#define DECLARE_SKIP_LIST_INSERT(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_node_t *skip_list_insert_ ## KEY_FIELD ## _ ## VALUE_FIELD(skip_list_t *l, element_t key, element_t value);


#define DECLARE_SKIP_LIST_FIND(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_node_t *skip_list_find_ ## KEY_FIELD ## _ ## VALUE_FIELD(skip_list_t *l, element_t ele);


#define DEF_SKIP_LIST_CREATE(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_list_t* skip_list_create_ ## KEY_FIELD ## _ ## VALUE_FIELD(){ \
    skip_list_t *slist = malloc(sizeof(*slist)); \
    slist->level = 1; \
    slist->length = 0; \
    skip_node_t *header = skip_node_create_ ## KEY_FIELD ## _ ## VALUE_FIELD(SKIPLIST_MAXLEVEL, (KEY_TYPE)0, (VALUE_TYPE)0); \
    header->backward = header; \
    for(int i=0; i<SKIPLIST_MAXLEVEL; i++){ \
        header->level[i].forward = header; \
        header->level[i].span = 0; \
    } \
    slist->header = header; \
    slist->print_func = &skip_list_print_ ## KEY_FIELD ## _ ## VALUE_FIELD; \
    slist->insert_func = &skip_list_insert_ ## KEY_FIELD ## _ ## VALUE_FIELD; \
    slist->find_func = &skip_list_find_ ## KEY_FIELD ## _ ## VALUE_FIELD; \
    return slist; \
} \



void skip_list_destroy(skip_list_t *l){
    skip_node_t *cur = l->header->level[0].forward;
    for(skip_node_t *next=cur->level[0].forward; cur!=l->header; cur=next, next=cur->level[0].forward){
        skip_node_destroy(cur);
    }
    skip_node_destroy(l->header);
    free(l);
}

static int random_level(void) {
    static const int threshold = SKIPLIST_P*RAND_MAX;
    int level = 1;
    while (random() < threshold)
        level += 1;
    return (level<SKIPLIST_MAXLEVEL) ? level : SKIPLIST_MAXLEVEL;
}


static inline int element_compare_i32(int32_t e1, int32_t e2){
    return e1>e2 ? 1 : (e1==e2 ? 0 : -1);
}

static inline int element_compare_u32(uint32_t e1, uint32_t e2){
    return e1>e2 ? 1 : (e1==e2 ? 0 : -1);
}

static inline int element_compare_i64(int64_t e1, int64_t e2){
    return e1>e2 ? 1 : (e1==e2 ? 0 : -1);
}

static inline int element_compare_u64(uint64_t e1, uint64_t e2){
    return e1>e2 ? 1 : (e1==e2 ? 0 : -1);
}

static inline int element_compare_s(char *s1, char *s2){
    return strcmp(s1, s2);
}


// skip_node_t *skip_list_insert(skip_list_t *l, int key, int value){
//     skip_node_t *update[SKIPLIST_MAXLEVEL] = {};
//     unsigned long rank[SKIPLIST_MAXLEVEL] = {};

//     int insert_level = random_level();
//     skip_node_t *node = skip_node_create(insert_level, key, value);

//     skip_node_t *cur = l->header;
//     for(int i=l->level-1; i>=0; i--){
//         rank[i] = i == (l->level-1) ? 0 : rank[i+1]; //累加上面一个level的span

//         //最后按照地址比较, 将地址小的放在前面, 相同key元素指针大小有序, 按照指针查找元素会很快
//         while(cur->level[i].forward != l->header && (cur->level[i].forward->key < key || (cur->level[i].forward->key == key && cur->level[i].forward < node))){
//             rank[i] += cur->level[i].span; //需要加上上header中的span, 所以先计算span
//             cur = cur->level[i].forward;
//         }
//         update[i] = cur;
//     }

//     if(insert_level > l->level){
//         for(int i=l->level; i<insert_level; i++){
//             rank[i] = 0;
//             update[i] = l->header;
//             update[i]->level[i].span = l->length;
//         }
//         l->level = insert_level;
//     }


//     for(int i=0; i<insert_level ; i++){
//         node->level[i].forward = update[i]->level[i].forward;

//         skip_node_t *prev = update[i];
//         prev->level[i].forward = node;

//         node->level[i].span = prev->level[i].span - (rank[0] - rank[i]);
//         prev->level[i].span = (rank[0] - rank[i])+1;
//     }

//     node->backward = update[0];
//     node->level[0].forward->backward = node;

//     for(int i=insert_level; i < l->level; i++){
//         update[i]->level[i].span++;
//     }

//     if(node->level[0].forward == l->header){
//         l->tail = node;
//     }

//     l->length++;

//     return node;
// }


#define DEF_SKIP_LIST_INSERT(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_node_t *skip_list_insert_ ## KEY_FIELD ## _ ## VALUE_FIELD(skip_list_t *l, element_t key, element_t value){ \
    skip_node_t *update[SKIPLIST_MAXLEVEL] = {}; \
    unsigned long rank[SKIPLIST_MAXLEVEL] = {}; \
    int insert_level = random_level();\
    skip_node_t *node = skip_node_create_ ## KEY_FIELD ## _ ## VALUE_FIELD(insert_level, key.KEY_FIELD, value.VALUE_FIELD); \
    skip_node_t *cur = l->header; \
    for(int i=l->level-1; i>=0; i--){ \
        rank[i] = i == (l->level-1) ? 0 : rank[i+1]; \
        while(cur->level[i].forward != l->header){ \
            int comp = element_compare_##KEY_FIELD(cur->level[i].forward->key.KEY_FIELD , key.KEY_FIELD); \
            if(comp < 0 || comp == 0 && cur->level[i].forward < node){ \
                rank[i] += cur->level[i].span;\
                cur = cur->level[i].forward; \
            }else{\
                break; \
            } \
        } \
        update[i] = cur; \
    } \
    if(insert_level > l->level){ \
        for(int i=l->level; i<insert_level; i++){ \
            rank[i] = 0; \
            update[i] = l->header; \
            update[i]->level[i].span = l->length; \
        } \
        l->level = insert_level; \
    } \
    for(int i=0; i<insert_level ; i++){ \
        node->level[i].forward = update[i]->level[i].forward; \
        skip_node_t *prev = update[i]; \
        prev->level[i].forward = node; \
        node->level[i].span = prev->level[i].span - (rank[0] - rank[i]); \
        prev->level[i].span = (rank[0] - rank[i])+1; \
    } \
    node->backward = update[0]; \
    node->level[0].forward->backward = node; \
    for(int i=insert_level; i < l->level; i++){ \
        update[i]->level[i].span++; \
    } \
    l->length++; \
    return node; \
} \



// skip_node_t *skip_list_find(skip_list_t *l, int key){
//     skip_node_t *cur = l->header;
//     for (int i = l->level-1; i >= 0; i--) {
//         while(cur->level[i].forward != l->header && cur->level[i].forward->key < key){
//             cur = cur->level[i].forward;
//         }

//         skip_node_t *next = cur->level[i].forward;
//         if(next != l->header && next->key == key){
//             return next;
//         }
//     }
//     return NULL;
// }


#define DEF_SKIP_LIST_FIND(KEY_TYPE, KEY_FIELD, VALUE_TYPE, VALUE_FIELD) \
skip_node_t *skip_list_find_ ## KEY_FIELD ## _ ## VALUE_FIELD(skip_list_t *l, element_t ele){ \
    KEY_TYPE key = ele.KEY_FIELD;  \
    skip_node_t *cur = l->header; \
    for (int i = l->level-1; i >= 0; i--) { \
        while(cur->level[i].forward != l->header){ \
            int comp = element_compare_##KEY_FIELD(cur->level[i].forward->key.KEY_FIELD, key); \
            if(comp < 0){ \
                cur = cur->level[i].forward; \
            }else { \
                break; \
            } \
        } \
    } \
    skip_node_t *next = cur->level[0].forward; \
    if(next != l->header && element_compare_##KEY_FIELD(next->key.KEY_FIELD, key) == 0){ \
        return next; \
    }else{ \
       return NULL; \
    } \
} \


// void skip_list_print(skip_list_t *l){
//     printf("list count: %lu, level is %d.\n", l->length, l->level);
//     for(int i=l->level-1; i>=0; i--){
//         printf("level %d: ", i);
//         for(skip_node_t *cur=l->header->level[i].forward; cur!=l->header; cur=cur->level[i].forward){
//             printf("%d(v%d)-", cur->key, cur->value);
//         }
//         printf("NULL\n");
//     }
// }


// void skip_list_rank_print(skip_list_t *l){
//     printf("list count: %lu, level is %d.\n", l->length, l->level);
//     for(int i=l->level-1; i>=0; i--){
//         printf("level %d(span%lu): ", i,l->header->level[i].span);
//         for(skip_node_t *cur=l->header->level[i].forward; cur!=l->header; cur=cur->level[i].forward){
//             printf("%d(span%lu)-", cur->key, cur->level[i].span);
//         }
//         printf("NULL\n");
//     }
// }

// void skip_list_addr_print(skip_list_t *l){
//     printf("list count: %lu, level is %d.\n", l->length, l->level);
//     for(int i=l->level-1; i>=0; i--){
//         printf("level %d(%p): ", i, l->header);
//         for(skip_node_t *cur=l->header->level[i].forward; cur!=l->header; cur=cur->level[i].forward){
//             printf("%d(addr%p)-", cur->key, cur);
//         }
//         printf("NULL\n");
//     }
// }

// int skip_list_remove(skip_list_t *l, int key){
//     skip_node_t *update[SKIPLIST_MAXLEVEL] = {};

//     skip_node_t *cur = l->header;
//     for(int i=l->level-1; i>=0; i--){
//         while(cur->level[i].forward != l->header && cur->level[i].forward->key < key){
//             cur = cur->level[i].forward;
//         }
//         update[i] = cur;
//     }

//     cur = cur->level[0].forward;
//     if(cur == l->header || cur->key != key){
//         return ENOENT;
//     }

//     for(int i=l->level-1; i>=0 ; i--){
//         skip_node_t *prev = update[i];
//         if(prev->level[i].forward == cur){
//             prev->level[i].span  += cur->level[i].span - 1;
//             prev->level[i].forward = cur->level[i].forward;
//         }else{
//             prev->level[i].span --;
//         }
//     }

//     skip_node_t *next = cur->level[0].forward;
//     next->backward = update[0];

//     if(next == l->header){
//         l->tail = update[0];
//     }

//     skip_node_destroy(cur);
//     l->length--;

//     while(l->level>1 && l->header->level[l->level-1].forward == l->header){
//         l->level--;
//     }

//     return 0;
// }

// int skip_list_remove_node(skip_list_t *l, skip_node_t *node){
//     // if(node == NULL || node == l->header){
//     //     return ENOENT;
//     // }

//     int key = node->key;

//     skip_node_t *update[SKIPLIST_MAXLEVEL] = {};

//     skip_node_t *cur = l->header;
//     for(int i=l->level-1; i>=0; i--){
//         while(cur->level[i].forward != l->header && (cur->level[i].forward->key < key || (cur->level[i].forward->key == key && cur->level[i].forward < node))){
//             cur = cur->level[i].forward;
//         }
//         update[i] = cur;
//     }

//     cur = cur->level[0].forward;
//     if(cur == l->header || cur != node){
//         return ENOENT;
//     }

//     //已经找到节点, cur == node
//     int i;
//     skip_node_t *prev;
//     for(i=l->level-1; i>=0 ; i--){
//         prev = update[i];

//         if(prev->level[i].forward == node){
//             prev->level[i].span  += cur->level[i].span - 1;
//             prev->level[i].forward = cur->level[i].forward;
//         }else{
//             prev->level[i].span --;
//         }
//     }

//     skip_node_t *next = node->level[0].forward;
//     next->backward = update[0];

//     if(next == l->header){
//         l->tail = update[0];
//     }

//     skip_node_destroy(node);
//     l->length--;

//     while(l->level>1 && l->header->level[l->level-1].forward == l->header){
//         l->level--;
//     }

//     return 0;
// }


// unsigned long skip_list_get_rank(skip_list_t *l, int key){
//     unsigned long rank = 0;
//     skip_node_t *cur = l->header;
    
//     for (int i = l->level-1; i >= 0; i--) {
//         while(cur->level[i].forward != l->header && cur->level[i].forward->key < key){
//             rank += cur->level[i].span;
//             cur = cur->level[i].forward;
//         }
//     }
    
//     rank += cur->level[0].span; // rank += 1
//     cur = cur->level[0].forward;

//     if(cur != l->header && cur->key == key){
//         return rank;
//     }else{
//         return 0;
//     }
// }


// unsigned long skip_list_get_node_rank(skip_list_t *l, skip_node_t *node){
//     // if(node == NULL || node == l->header){
//     //     return ENOENT;
//     // }

//     unsigned long rank = 0;
//     int key = node->key;
//     skip_node_t *cur = l->header;
    
//     for (int i = l->level-1; i >= 0; i--) {
//         while(cur->level[i].forward != l->header && (cur->level[i].forward->key < key ||( cur->level[i].forward->key == key && cur->level[i].forward <= node ))){
//             rank += cur->level[i].span;
//             cur = cur->level[i].forward;
//         }
//     }

//     if(cur != l->header && cur->key == key){
//         return rank;
//     }else{
//         return 0;
//     }
// }


// skip_node_t *skip_list_get_by_rank(skip_list_t *l, unsigned long rank){
//     unsigned long traversed = 0;
//     skip_node_t *cur = l->header;

//     for (int i = l->level-1; i >= 0; i--) {
//         while (cur->level[i].forward && (traversed + cur->level[i].span) <= rank)
//         {
//             traversed += cur->level[i].span;
//             cur = cur->level[i].forward;
//         }
//         if (traversed == rank) {
//             return cur;
//         }
//     }
//     return NULL;
// }


DEF_SKIP_LIST_PRINT(int32_t, i32, int32_t, i32)
DEF_SKIP_NODE_CREATE(int32_t, i32, int32_t, i32)
DECLARE_SKIP_LIST_INSERT(int32_t, i32, int32_t, i32)
DECLARE_SKIP_LIST_FIND(int32_t, i32, int32_t, i32)
DEF_SKIP_LIST_CREATE(int32_t, i32, int32_t, i32)
DEF_SKIP_LIST_INSERT(int32_t, i32, int32_t, i32)
DEF_SKIP_LIST_FIND(int32_t, i32, int32_t, i32)


#define K 1000
#define M (1000*1000)


int main(){
    skip_list_t *i32_skiplist =  skip_list_create_i32_i32();

    element_t key;
    element_t value;
    int num_list[20];
    for(int i=0; i<20; i++){
        key.i32 = random() % 100;
        value.i32 = -key.i32;
        i32_skiplist->insert_func(i32_skiplist, key, value);
        // skip_node_t *node = skip_list_insert_i32_i32(i32_skiplist, key, value);
    }

    i32_skiplist->print_func(i32_skiplist);
    // skip_list_print_i32_i32(i32_skiplist);

    skip_node_t *node;
    key.i32 = 56;
    // node = skip_list_find_i32_i32(i32_skiplist, key);
    node = i32_skiplist->find_func(i32_skiplist, key);
    if(node != NULL){
        fprintf(stderr, "found key: %d, value is: %d\n", node->key, node->value);
    }else{
        fprintf(stderr, "not found\n");
    }

    skip_list_destroy(i32_skiplist);


    // skip_list_rank_print(sl);
    // skip_list_addr_print(sl);


    // int delete_ele[] = {11,22,33,3,3,3,5,7,7,7,19,19,-1};
    // for(int i=0; delete_ele[i]!=-1; i++){
    //     int ret = skip_list_remove(sl, delete_ele[i]);
    //     fprintf(stderr, "remove: %d %s\n", delete_ele[i], ret==0?"success":"failed");
    // }
    // fprintf(stderr, "skiplist count is %lu, level is %d.\n", sl->length, sl->level);



    // fprintf(stderr, "==== test print\n");
    // skip_list_rank_print(sl);
    // skip_list_addr_print(sl);

    // fprintf(stderr, "==== test reverse\n");
    // skip_list_foreach_reverse(node, sl) {
    //     fprintf(stderr, "%d-", node->key);
    // }
    // fprintf(stderr, "\n\n");


    // fprintf(stderr, "test skip_list_for_each_reverse_safe\n");
    // skip_list_foreach_reverse_safe(node, sl){
    //     if(node->key == 6){
    //         int ret = skip_list_remove_node(sl, node);
    //         fprintf(stderr, "delete node %p key 6 : %s\n", node, ret == 0? "ok": "failed");
    //     }
    // }
    // skip_list_rank_print(sl);

    // printf("skip_list_get_rank(17) == %d \n", skip_list_get_rank(sl, 17));

    // printf("skip_list_get_node_rank(17) == %d \n", skip_list_get_node_rank(sl, skip_list_find(sl, 17)));

    // printf("skip_list_get_by_rank(15).value == %d \n", skip_list_get_by_rank(sl, 15)->value);


    // fprintf(stderr, "test skip_list_for_each_safe\n");
    // skip_list_foreach_safe(node, sl){
    //     fprintf(stderr, "%d-", node->key);
    // }

    return 0;
}

