#include <solution.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
struct Node {
    bool leaf; 
    uint t; //minimum degree
    uint n; // current number of keys
    int *key; // keys
    struct Node **children; // child pointers
};

struct btree {
    uint t; // minimum degree
    struct Node *root; // pointer to root node
};
struct btree* btree_alloc(unsigned int L) {
    struct btree *tree = (struct btree*)calloc(1, sizeof(struct btree));
    tree->root = NULL;
    tree->t = L;
    return tree;
}
struct Node* node_alloc(bool leaf, uint L) {
    struct Node *root = (struct Node*)calloc(1, sizeof(struct Node));    
    root->leaf = leaf;
    root->t = L;
    root->n = 0;
    root->key = (int*)calloc((2 * L - 1), sizeof(int));
    root->children = (struct Node**)calloc((2 * L), sizeof(struct Node*));
    return root;
}

void destroy_node(struct Node* tree) {
    free(tree->children);
    free(tree->key);
    free(tree);
}
void node_free(struct Node* tree) {
    if (tree == NULL) return;
    // printf("%d\n", 2 * tree->t);
    if (!tree->leaf) {
        for (long int i = 0; i < tree->n + 1; ++i) {
            node_free(tree->children[i]);
            // tree->children[i] = NULL;
        }
    }
    free(tree->children);       
    // for(long int i = 0; i < tree->n;i++){
    //         printf("del: %d ",tree->key[i]);
    //     }
    free(tree->key); 
    free(tree);
}
void btree_free(struct btree *t) {
    // if (t->root) 
    node_free(t->root);
    free(t);
}
void print_tree(struct btree*t) {
    print(t->root);
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
            i--;
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
        // T->root->children = NULL;
        return ;
    } 
    int t = T->t;    
    struct Node *r = T->root;    
    if((long int) r->n == (long int)2 * t - 1) {
        struct Node *s = node_alloc(0, t);
        // T->root = s;
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
    for (long int i = 0; i < T->t - 1; i++) {
        left->key[i + T->t] = right->key[i];
    }
    if (!left->leaf) {
        for (long int i = 0; i < T->t; i++) {
            left->children[i + T->t] = right->children[i];
        }
    }
    left->n += T->t;

    destroy_node(right);

    for (long int j = idx + 1; j < node->n; j++) {
        node->key[j - 1] = node->key[j];
        node->children[j] = node->children[j + 1];
    }
    node->key[node->n - 1] = 0;
    node->children[node->n] = NULL;
    node->n--;

    if (!node->n) {
        T->root = left;
        destroy_node(node);
    }
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
                while (cur->leaf == 0) {
                    cur = cur->children[cur->n];
                }
                node->key[idx] = cur->key[cur->n - 1];
                btree_delete_key(T, node->children[idx], node->key[idx]);
            } else if (node->children[idx + 1]->n >= node->t) {
                struct Node* cur = node->children[idx + 1];
                while (cur->leaf == 0) {
                    cur = cur->children[0];
                }
                node->key[idx] = cur->key[0];
                btree_delete_key(T, node->children[idx + 1], node->key[idx]);
            } else {
                btree_merge_key(T, node, idx);
                btree_delete_key(T, node->children[idx], x);
            }            
        }
    } else {
        struct Node* child = node->children[idx];
        if (node == NULL) {
            return;
        }
        if (child->n == T->t - 1) { // If the current child node is not enough to borrow
            struct Node* left = node->children[idx - 1];
            struct Node* right = node->children[idx + 1];
            if ((left && left->n >= T->t) || // Ask the brother node to borrow, then add it to the child node, and then delete it
                (right && right->n >= T->t)) {
                int richR = 0;
                if (right) {
                    richR = 1;
                }
                if (left && right) {
                    richR = (right->n - left->n) ? 1 : 0;
                }

                if (right && right->n >= T->t && richR) {
                    child->key[child->n] = node->key[idx];
                    child->children[child->n + 1] = right->children[0];
                    child->n += 1;

                    node->key[idx] = right->key[0];
                    for (long int i = 0; i < right->n - 1; i++) {
                        right->key[i] = right->key[i + 1];
                        right->children[i] = right->children[i + 1];
                    }
                    right->key[right->n - 1] = 0;
                    right->children[right->n - 1] = right->children[right->n];
                    right->children[right->n] = NULL;
                    right->n--; 
                } else {
                    for (long int j = child->n; j > 0; j--) {
                        child->key[j] = child->key[j - 1];
                        child->children[j + 1] = child->children[j];
                    }
                    child->key[0] = left->key[left->n - 1];
                    child->children[1] = child->children[0];
                    child->children[0] = left->children[left->n];
                    child->n++;

                    left->children[left->n] = NULL;
                    left->key[left->n - 1] = 0;
                    left->n--;
                }
            } else if ((!left || left->n == T->t - 1) ||
                       (!right || right->n == T->t - 1)) {
                if (left && left->n == T->t - 1) {
                    btree_merge_key(T, node, idx -1);
                    child = left;
                } else if (right && right->n == T->t - 1) {
                    btree_merge_key(T, node, idx);
                }

            }
        }
        btree_delete_key(T, child, x);
    }

}

void btree_delete(struct btree *t, int x) {
    if (!btree_contains(t, x)) return;
    if (!t->root) return;
    btree_delete_key(t, t->root, x);
}
bool node_contains(struct Node* node, int x) {
    if (!node) return false;
    long int i = 0;
    while (i < node->n && x > node->key[i]) ++i;
    if (i < node->n && node->key[i] == x) return true;
    if (node->leaf) return false;
    return node_contains(node->children[i], x);
}

bool btree_contains(struct btree *t, int x) {
    if (!t->root) return false;
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
   struct btree* tree;
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
