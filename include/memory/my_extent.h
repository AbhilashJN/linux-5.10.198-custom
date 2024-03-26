#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/rbtree.h>

// represents one page in the linked list inside an extent
struct extent_pg_node {
	unsigned long ppn;
	unsigned long vpn;
	struct list_head list_ref;
};

// represents an extent of physically and virtually contiguous pages
struct extent {
	int extent_id;
	int page_count;
	unsigned long first_vpn;
	unsigned long first_ppn;
	unsigned long last_vpn;
	unsigned long last_ppn;
	struct list_head page_list;
	struct rb_node rbnode;
};

// red-black tree used for storing extents
struct extents_tree {
	spinlock_t lock;
	struct rb_root root;
};

// create new page in extent with given virtual and physical page numbers
struct extent_pg_node *new_extent_pg_node(unsigned long vpn, unsigned long ppn);

// create new with given id
struct extent *new_extent(int extent_id);

// add a new page to the end of the given extent
void add_page_to_extent_tail(struct extent_pg_node *node,
				    struct extent *extent);

// add a new page to the beginning of the given extent
void add_page_to_extent_head(struct extent_pg_node *node,
				    struct extent *extent);

// print the page numbers in the given extent
void print_extent_pg_list(struct extent *extent);

// insert a new page into the extents RB tree
// page might either be added to one of the existing extents in the tree
// or a new extent with just this page will be created and added in the tree
void insert_my_extent(struct rb_root *root, struct extent_pg_node *new_page);

// print the contents of the extents RB tree.
void print_extent_tree(struct rb_root *root);