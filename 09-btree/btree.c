#include <solution.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
struct Node {
    bool leaf; 
    int t; //minimum degree
    int n; // current number of keys
    int *key; // keys
    struct Node **children; // child pointers
};

struct btree {
    int t; // minimum degree
    struct Node *root; // pointer to root node
};
struct btree* btree_alloc(unsigned int L) {
    struct btree *tree = (struct btree*)calloc(1, sizeof(struct btree));
    tree->root = NULL;
    tree->t = L+2;
    return tree;
}
struct Node* node_alloc(bool leaf, uint L) {
    struct Node *root = (struct Node*)calloc(1, sizeof(struct Node));    
    root->leaf = leaf;
    root->t = L;
    root->n = 0;
    root->key = (int*)calloc((2 * L + 1), sizeof(int));
    root->children = (struct Node**)calloc((2 * L + 1), sizeof(struct Node*));
    return root;
}

void destroy_node(struct Node* tree) {
    free(tree->children);
    free(tree->key);
    free(tree);
}
void node_free(struct Node* tree) {
    if (tree == NULL) return;
    if (!tree->leaf) {
        for (long int i = 0; i < tree->n + 1; ++i) {
            node_free(tree->children[i]);
        }
    }
    free(tree->children);      
    free(tree->key); 
    free(tree);
}
void btree_free(struct btree *t) {
    // if (t->root) 
    node_free(t->root);
    free(t);
}
void print(struct Node *tree) {
    if (!tree) return;
    if(tree->leaf){
        // printf("root ");
        for(long int i = 0; i < tree->n;i++){
            printf(" %d ",tree->key[i]);
        }
        printf("\n");
    }
    else {
        for(long int i = 0; i < tree->n; i++){
            printf("r: %d ",tree->key[i]);
        }
        printf("\n");
        for(long int i = 0; i < tree->n; i++){
            print(tree->children[i]);
        }
        print(tree->children[tree->n]);
    }
}
void print_tree(struct btree*t) {
    print(t->root);
}

void btree_split_child(struct Node *x, int i) {    
    int t = x->t;    
    struct Node *y = x->children[i];
    struct Node *z = node_alloc(y->leaf, t);
    z->n = t - 1;
    int j;
    for(j = 0; j < t-1; j++) {
        z->key[j] = y->key[j+t];
    }
    if(y->leaf == 0) {
        for(j = 0; j < t; j++) {
            z->children[j] = y->children[j+t];
        }
    }    
    y->n = t-1; 
    for(j = x->n; j >= i+1; j--) {
        x->children[j+1] = x->children[j];
    }
    x->children[i+1] = z;
    for(j = x->n - 1; j >= i; j--) {
        x->key[j+1] = x->key[j];
    }
    x->key[i] = y->key[t-1];
    x->n += 1;    
}

void btree_insert_non_full(struct Node *x, int k) {    
    int i = x->n - 1;    
    if(x->leaf == 1) {
        while(i >= 0 && x->key[i] > k) {
            x->key[i+1] = x->key[i];
            --i;
        }
        x->key[i+1] = k;
        x->n += 1;
    }
    else {
        while(i >= 0 && x->key[i] > k) i--;
        if(x->children[i + 1 ]->n == 2 * x->t - 1) {
            btree_split_child(x, i + 1);
            if(k > x->key[i + 1]) i++;
        }
        btree_insert_non_full(x->children[i + 1], k);
    }    
}

void btree_insert(struct btree *T, int k) {
    if (btree_contains(T, k)) return;
    if (T->root == NULL) {
        //allocate memory for root
        T->root = node_alloc(true, T->t);
        T->root->key[0] = k;
        T->root->n = 1;
        return ;
    } 
    int t = T->t;    
    struct Node *r = T->root;    
    if((long int) r->n == (long int)2 * t - 1) {
        struct Node *s = node_alloc(0, t);
        s->children[0] = r;
        btree_split_child(s, 0);
        int i = 0;
        if (s->key[0] < k) i++;
        btree_insert_non_full(s->children[i], k);
        T->root = s;
    }
    else {
        btree_insert_non_full(r, k);
    }
}


void btree_merge_key(struct btree* T, struct Node* node, int idx) {
    if (T->root == NULL || node == NULL) {
        return;
    }
    struct Node* left = node->children[idx];
    struct Node* right = node->children[idx + 1];

    left->key[T->t - 1] = node->key[idx];
    for (long int i = 0; i < right->n; i++) {
        left->key[i + T->t] = right->key[i];
    }
    if (!left->leaf) {
        for (long int i = 0; i < right->n + 1; i++) {
            left->children[i + T->t] = right->children[i];
        }
    }
    for (long int j = idx + 1; j < node->n; j++) {
        node->key[j - 1] = node->key[j];
    }
    for (long int j = idx + 2; j < node->n + 1; j++) {
        node->children[j - 1] = node->children[j];
    }
    left->n += right->n + 1;
    --(node->n);
    destroy_node(right);
}
void btree_delete_key(struct btree* T, struct Node* node, int x) {
    if (node == NULL) {
        return;
    }
    long int idx = 0;
	while (idx < node->n && x > node->key[idx]) { // find the first value greater than key
		idx++;
	}
    if (idx < node->n && x == node->key[idx]) {  // if the key is found
        if (node->leaf) { // The node is a leaf node
            for (long int i = idx + 1; i < node->n; ++i) {
                node->key[i - 1] = node->key[i];
            }
            --(node->n);
        } else {
            if (node->children[idx]->n >= node->t) {
                struct Node* cur = node->children[idx];
                while (!cur->leaf) {
                    cur = cur->children[cur->n];
                }
                node->key[idx] = cur->key[cur->n - 1];
                int val = node->key[idx];
                btree_delete_key(T, node->children[idx], val);
            } else if (node->children[idx + 1]->n >= node->t) {
                struct Node* cur = node->children[idx + 1];
                while (!cur->leaf) {
                    cur = cur->children[0];
                }
                node->key[idx] = cur->key[0];
                int val = node->key[idx];
                btree_delete_key(T, node->children[idx + 1], val);
            } else {
                btree_merge_key(T, node, idx);
                btree_delete_key(T, node->children[idx], x);
            }            
        }
    } else {
        if (node->leaf) return;
        bool equal_cnt = false;
        if (idx == node->n) equal_cnt = true;
        // Ask the brother node to borrow, then add it to the child node, and then delete i
        if (node->children[idx]->n < T->t) {
            if (idx != 0 && node->children[idx - 1]->n >= T->t) {
                struct Node *left = node->children[idx];
                struct Node *right = node->children[idx - 1];
                if (!left->leaf) {
                    for (long int i = left->n; i >= 0; i--) {
                        left->children[i + 1] = left->children[i];
                    }
                }
                for (int i = left->n - 1; i >= 0; --i) {
                    left->key[i +1] = left->key[i];
                }
                left->key[0] = node->key[idx - 1];
                if (!left->leaf) {
                    left->children[0] = right->children[right->n];
                }
                node->key[idx - 1] = right->key[right->n - 1];
                ++ left->n;
                --right->n;
            } else if (idx != node->n && node->children[idx + 1]->n >= T->t) {
                struct Node* left = node->children[idx];
                struct Node* right = node->children[idx + 1];
                left->key[left->n] = right->key[idx];
                if (!left->leaf) {
                    left->children[left->n + 1] = right->children[0];
                }
                node->key[idx] = right->key[0];
                for (long int i = 1; i < right->n; ++i) {
                    right->key[i - 1] = right->key[i];
                }
                if (!right->leaf) {
                    for (long int i = 1; i < right->n + 1; ++i) {
                        right->children[i - 1] = right->children[i];
                    }
                }
                ++left->n;
                --right->n;
            } else {
                if (idx != node->n) {
                    btree_merge_key(T, node, idx);
                } else {
                    btree_merge_key(T, node, idx - 1);
                }
            }
        }
        if (equal_cnt && idx > node->n) {
			btree_delete_key(T, node->children[idx - 1], x);
		} else {
			btree_delete_key(T, node->children[idx], x);
		}
    }

}

void btree_delete(struct btree *t, int x) {
    if (!btree_contains(t, x)) return;
    if (!t->root) return;
    btree_delete_key(t, t->root, x);
    if (t->root->n == 0) {
        struct Node*root = t->root;
        if(t->root->leaf) t->root = NULL;
        else t->root = t->root->children[0];
        destroy_node(root);
    }
}
bool node_contains(struct Node* node, int x) {
    if (!node) return false;
    if (node->leaf) return false;
    long int i = 0;
    while (i < node->n && x > node->key[i]) ++i;
    if (i < node->n && node->key[i] == x) return true;
    return node_contains(node->children[i], x);
}

bool btree_contains(struct btree *t, int x) {
    return node_contains(t->root, x);
}
struct btree_iter;

int count(struct Node *tree) {
    if (!tree) return 0;
    int cnt = tree->n;
    if(tree->leaf){
    }
    else {
        for(long int i = 0; i <= tree->n; i++){
            cnt += count(tree->children[i]);
        }
    }
    return cnt;
}

void traverse(struct Node* node, int *it, int *idx) {
    if (!node) return;
    for (long int i = 0; i < node->n; ++i) {
        if (!node->leaf) {
            traverse(node->children[i], it, idx);
        }
        it[*idx] = node->key[i];
        ++(*idx);
    }
    if (!node->leaf) {
        traverse(node->children[node->n], it, idx);
    }
}

struct btree_iter {
   int *values;
   int cnt;
   int i;
};
struct btree_iter* btree_iter_start(struct btree *t)
{
    struct btree_iter *it =(struct btree_iter*) calloc(1, sizeof(struct btree_iter));
    int cnt = count(t->root);
    it->cnt = cnt;
    it->values = calloc(cnt, sizeof(int));
    int idx = 0;
    traverse(t->root, it->values, &idx);
    return it;
}

void btree_iter_end(struct btree_iter *i) {
    free(i->values);
    free(i);
}

bool btree_iter_next(struct btree_iter *i, int *x){
    if (i->i == i->cnt) return false;
    *x = i->values[i->i];
    ++i->i;
    return true;
}
