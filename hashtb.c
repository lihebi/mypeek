/**
 * @file hashtb.c
 * @brief Hash table.
 *
 * Part of the NDNx C Library.
 *
 * Portions Copyright (C) 2013 Regents of the University of California.
 *
 * Based on the CCNx C Library by PARC.
 * Copyright (C) 2009 Palo Alto Research Center, Inc.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details. You should have received
 * a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ndn/hashtb.h>

struct node;
struct node {
    struct node* link;
    size_t hash;
    size_t keysize;
    size_t extsize;
    /* user data follows immediately, followed by key */
};
#define DATA(ht, p) ((void *)((p) + 1))
#define KEY(ht, p) ((unsigned char *)((p) + 1) + ht->item_size)

#define CHECKHTE(ht, hte) ((uintptr_t)((hte)->priv[1]) == ~(uintptr_t)(ht))
#define MARKHTE(ht, hte) ((hte)->priv[1] = (void*)~(uintptr_t)(ht))

struct hashtb {
    struct node **bucket;       /* hash桶 */
    size_t item_size;           /* Size of client's per-entry data */
    unsigned n_buckets;         /* 位置桶的容量 */
    int n;                      /* 入口数量 Number of entries */
    int refcount;               /* 活跃的迭代器数量 Number of open enumerators */
    struct node *deferred;      /* 延后的清理工作 deferred cleanup */
    struct hashtb_param param;  /* 保存的客户端参数 saved client parameters */
};

// 对key和keysize做哈西
size_t
hashtb_hash(const unsigned char *key, size_t key_size)
{
    size_t h;
    size_t i;
    for (h = key_size + 23, i = 0; i < key_size; i++)
        h = ((h << 6) ^ (h >> 27)) + key[i];
    return(h);
}

// 创建hashtb时根据指定的size。所以可以自定义table的类型。
struct hashtb *
hashtb_create(size_t item_size, const struct hashtb_param *param)
{
    struct hashtb *ht;
    ht = calloc(1, sizeof(*ht));
    if (ht != NULL) {
        ht->item_size = item_size;
        ht->n = 0;
        ht->n_buckets = 7;
        ht->bucket = calloc(ht->n_buckets, sizeof(ht->bucket[0]));
	if (ht->bucket == NULL) {
		free(ht);
		return (NULL); /*ENOMEM*/
	}
        if (param != NULL)
            ht->param = *param;
    }
    return(ht);
}

void *
hashtb_get_param(struct hashtb *ht, struct hashtb_param *param)
{
    if (param != NULL)
        *param = ht->param;
    return(ht->param.finalize_data);
}

void
hashtb_destroy(struct hashtb **htp)
{
    if (*htp != NULL) {
        struct hashtb_enumerator tmp;
        struct hashtb_enumerator *e = hashtb_start(*htp, &tmp);
        while (e->key != NULL)
            hashtb_delete(e);
        hashtb_end(&tmp);
        if ((*htp)->refcount == 0) {
            free((*htp)->bucket);
            free(*htp);
            *htp = NULL;
        }
        else
            abort(); /* perhaps a bit brutal... */
    }
}

// 返回入口数量
int
hashtb_n(struct hashtb *ht)
{
    return(ht->n);
}

// 参数：
// 1. table
// 2. key
// 3. keysize
void *
hashtb_lookup(struct hashtb *ht, const void *key, size_t keysize)
{
    struct node *p;
    size_t h;
    if (key == NULL)
        return(NULL);
    // 对key和keysize做哈希
    h = hashtb_hash(key, keysize);
    for (p = ht->bucket[h % ht->n_buckets]; p != NULL; p = p->link) {
        if (p->hash < h)
            continue;
        if (p->hash > h)
        // 失败！！！
            break;
        // 找到！！！
        if (keysize == p->keysize && 0 == memcmp(key, KEY(ht, p), keysize))
            return(DATA(ht, p));
    }
    return(NULL);
}

static void
setpos(struct hashtb_enumerator *hte, struct node **pp)
{
    struct hashtb *ht = hte->ht;
    struct node *p = NULL;
    hte->priv[0] = pp;
    if (pp != NULL)
        p = *pp;
    if (p == NULL) {
        hte->key = NULL;
        hte->keysize = 0;
        hte->extsize = 0;
        hte->data = NULL;
    }
    else {
        hte->key = KEY(ht, p);
        hte->keysize = p->keysize;
        hte->extsize = p->extsize;
        hte->data = DATA(ht, p);
    }
}

// 第b个节点的node结构体
static struct node **
scan_buckets(struct hashtb *ht, unsigned b)
{
    for (; b < ht->n_buckets; b++)
        if (ht->bucket[b] != NULL)
            return &(ht->bucket[b]);
    return(NULL);
}

#define MAX_ENUMERATORS 30
// 获取table头节点的迭代器。会将第二个参数改变，也会返回同样的值。
struct hashtb_enumerator *
hashtb_start(struct hashtb *ht, struct hashtb_enumerator *hte)
{
    MARKHTE(ht, hte);
    hte->datasize = ht->item_size;
    hte->ht = ht;
    ht->refcount++;
    if (ht->refcount > MAX_ENUMERATORS)
        abort(); /* probably somebody is missing a call to hashtb_end() */
    // 把迭代器位置设为开头
    setpos(hte, scan_buckets(ht, 0));
    return(hte);
}

void
hashtb_end(struct hashtb_enumerator *hte)
{
    struct hashtb *ht = hte->ht;
    struct node *p;
    hashtb_finalize_proc f;
    if (!CHECKHTE(ht, hte) || ht->refcount <= 0) abort();
    if (ht->refcount == 1) {
        /* do deferred deallocation */
        f = ht->param.finalize;
        while (ht->deferred != NULL) {
            setpos(hte, &(ht->deferred));
            if (f != NULL)
                (*f)(hte);
            p = ht->deferred;
            ht->deferred = p->link;
            free(p);
        }
    }
    hte->priv[0] = 0;
    hte->priv[1] = 0;
    ht->refcount--;
}

void
hashtb_next(struct hashtb_enumerator *hte)
{
    struct node **pp = hte->priv[0];
    struct node **ppp;
    if (pp != NULL) {
        ppp = pp;
        pp = &((*pp)->link);
        if (*pp == NULL)
           pp = scan_buckets(hte->ht, ((*ppp)->hash % hte->ht->n_buckets) + 1);
    }
    setpos(hte, pp);
}

// 把迭代器定位到key和keysize所指示的node节点上。
int
hashtb_seek(struct hashtb_enumerator *hte, const void *key, size_t keysize, size_t extsize)
{
    struct node *p = NULL;
    struct hashtb *ht = hte->ht;
    struct node **pp;
    size_t h;
    if (key == NULL) {
        setpos(hte, NULL);
        return(-1);
    }
    if (ht->refcount == 1 && ht->n > ht->n_buckets * 3) {
        ht->refcount--;
        hashtb_rehash(ht, 2 * ht->n + 1);
        ht->refcount++;
    }
    h = hashtb_hash(key, keysize);
    pp = &(ht->bucket[h % ht->n_buckets]);
    for (p = *pp; p != NULL; pp = &(p->link), p = p->link) {
        if (p->hash < h)
            continue;
        if (p->hash > h)
            break;
        if (keysize == p->keysize && 0 == memcmp(key, KEY(ht, p), keysize)) {
            setpos(hte, pp);
            return(HT_OLD_ENTRY);
        }
    }
    p = calloc(1, sizeof(*p) + ht->item_size + keysize + extsize);
    if (p == NULL) {
        setpos(hte, NULL);
        return(-1);
    }
    memcpy(KEY(ht, p), key, keysize + extsize);
    p->hash = h;
    p->keysize = keysize;
    p->extsize = extsize;
    p->link = *pp;
    *pp = p;
    hte->ht->n += 1;
    setpos(hte, pp);
    return(HT_NEW_ENTRY);
}

void
hashtb_delete(struct hashtb_enumerator *hte)
{
    struct hashtb *ht = hte->ht;
    struct node **pp = hte->priv[0];
    struct node *p = *pp;
    if ((p != NULL) && CHECKHTE(ht, hte) && KEY(ht, p) == hte->key) {
        *pp = p->link;
        if (*pp == NULL)
           pp = scan_buckets(hte->ht, (p->hash % hte->ht->n_buckets) + 1);
        hte->ht->n -= 1;
        if (ht->refcount == 1) {
            hashtb_finalize_proc f = ht->param.finalize;
            if (f != NULL)
                (*f)(hte);
            free(p);
        }
        else {
            p->link = ht->deferred;
            ht->deferred = p;
        }
        setpos(hte, pp);
    }
}

void
hashtb_rehash(struct hashtb *ht, unsigned n_buckets)
{
    struct node **bucket = NULL;
    struct node **pp;
    struct node *p;
    struct node *q;
    size_t h;
    unsigned i;
    unsigned b;
    if (ht->refcount != 0 || n_buckets < 1 || n_buckets == ht->n_buckets)
        return;
    bucket = calloc(n_buckets, sizeof(bucket[0]));
    if (bucket == NULL) return; /* ENOMEM */
    for (i = 0; i < ht->n_buckets; i++) {
        for (p = ht->bucket[i]; p != NULL; p = q) {
            q = p->link;
            h = p->hash;
            b = h % n_buckets;
            for (pp = &bucket[b]; *pp != NULL && ((*pp)->hash < h); pp = &((*pp)->link))
                continue;
            p->link = *pp;
            *pp = p;
        }
    }
    free(ht->bucket);
    ht->bucket = bucket;
    ht->n_buckets = n_buckets;
}
