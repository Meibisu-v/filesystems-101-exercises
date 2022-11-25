#include <solution.h>
#include <stdio.h>
struct btree;
int main()
{	
	// struct btree *t = btree_alloc(1);
	// btree_insert(t, 0);
	// btree_insert(t, 1);
	// // btree_insert(t, 2);
	// print_tree(t);
	// btree_delete(t, 1);
	// printf("bool  %d ",btree_contains(t, 1));
	// print_tree(t);
	// struct btree_iter *i = btree_iter_start(t);
	// int x;
	// for (;btree_iter_next(i, &x);)
	// 	printf("%i\n", x);
	// btree_iter_end(i);

	// btree_free(t);
	// return 0;
	struct btree *t = btree_alloc(2);
	btree_insert(t, 5);
	btree_insert(t, 9);
	btree_insert(t, 3);
	btree_insert(t, 7);
	btree_insert(t, 1);
	btree_insert(t, 2);
	btree_insert(t, 8);
	btree_insert(t, 6);
	btree_insert(t, 0);
	btree_insert(t, 4);
	
	print_tree(t);
	btree_delete(t, 5);
	print_tree(t);
	printf("find (5) = %d, find(42) = %d\n", btree_contains(t, 7), btree_contains(t, 42));
	
	btree_insert(t, 5);
	struct btree_iter *i = btree_iter_start(t);
	int x;
	btree_iter_next(i, &x);
	for (;btree_iter_next(i, &x);)
		printf("%i\n", x);
	btree_iter_end(i);
	// print(t->root);
	btree_free(t);
	return 0;
}
