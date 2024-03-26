#include <memory/my_extent.h>

struct extent_pg_node *new_extent_pg_node(unsigned long vpn, unsigned long ppn)
{
	struct extent_pg_node *n = kmalloc(sizeof(*n), GFP_ATOMIC);
	n->vpn = vpn;
	n->ppn = ppn;
	INIT_LIST_HEAD(&(n->list_ref));
	return n;
}

struct extent *new_extent(int extent_id)
{
	struct extent *e = kmalloc(sizeof(*e), GFP_ATOMIC);
	e->extent_id = extent_id;
	e->first_vpn = -1;
	e->first_ppn = -1;
	e->last_vpn = -1;
	e->last_ppn = -1;
	e->page_count = 0;
	INIT_LIST_HEAD(&(e->page_list));

	return e;
}

void add_page_to_extent_tail(struct extent_pg_node *node,
				    struct extent *extent)
{
	list_add_tail(&(node->list_ref), &(extent->page_list));
	extent->last_vpn = node->vpn;
	extent->last_ppn = node->ppn;

	if (extent->first_vpn == -1) {
		extent->first_vpn = node->vpn;
		extent->first_ppn = node->ppn;
	}
	extent->page_count++;
}

void add_page_to_extent_head(struct extent_pg_node *node,
				    struct extent *extent)
{
	list_add(&(node->list_ref), &(extent->page_list));
	extent->first_vpn = node->vpn;
	extent->first_ppn = node->ppn;

	if (extent->last_vpn == -1) {
		extent->last_vpn = node->vpn;
		extent->last_ppn = node->ppn;
	}
	extent->page_count++;
}

void print_extent_pg_list(struct extent *extent)
{
	printk("Pages in extent %d: %lu,%lu -> %lu,%lu\n", extent->extent_id,
	       extent->first_vpn, extent->first_ppn, extent->last_vpn,
	       extent->last_ppn);
	// struct extent_pg_node *n = NULL;
	// list_for_each_entry (n, &(extent->page_list), list_ref) {
	// 	printk("page addr = %lu\n", n->ppn);
	// 	printk("(%lu,%lu) -> ", n->vpn, n->ppn);
	// }
	printk("\n");
}


extern int my_extent_id;

// must be holding the rb tree lock when entering.
// This lock is still held when returning from this function.
void insert_my_extent(struct rb_root *root, struct extent_pg_node *new_page)
{
	struct rb_node **curr = &(root->rb_node), *parent = NULL;

	/* Traverse the tree to figure out where to put new extent */
	while (*curr) {
		struct extent *this_extent = rb_entry(*curr, struct extent, rbnode);
		parent = *curr;

		// check if the new page can be appended to the beginning of this extent.
		if (new_page->vpn == this_extent->first_vpn - 1 &&
		    new_page->ppn == this_extent->first_ppn - 1) {
			add_page_to_extent_head(new_page, this_extent);

			// check if this extent can now be merged with the
			// previous extent in the tree.
			struct rb_node *prev_rb_node = rb_prev(*curr);
			if (prev_rb_node == NULL) { // there is no previous extent in the tree, so return
				return;
			}

			struct extent *prev_extent = rb_entry(prev_rb_node, struct extent, rbnode);
			
			// if the end of the previous extent does not meet the
			// beginning of this current extent, we cannot merge them
			// so return.
			if (prev_extent->last_vpn != this_extent->first_vpn - 1 
			   || prev_extent->last_ppn != this_extent->first_ppn - 1) {
				return;
			}

			// add all pages from this extent to the tail of previous extent
			list_splice_tail(&this_extent->page_list,
					 &prev_extent->page_list);
			
			prev_extent->page_count += this_extent->page_count;
			prev_extent->last_vpn = this_extent->last_vpn;
			prev_extent->last_ppn = this_extent->last_ppn;

			// remove this extent from the tree
			rb_erase(*curr, root);
			return;
		}

		//check if the new page can be appended to the end of this extent.
		if (new_page->vpn == this_extent->last_vpn + 1 &&
		    new_page->ppn == this_extent->last_ppn + 1) {
			add_page_to_extent_tail(new_page, this_extent);

			// check if this extent can now be merged with the
			// next extent in the tree.
			struct rb_node *next_rb_node = rb_next(*curr);
			if (next_rb_node == NULL) { // there is no previous extent in the tree, so return
				return;
			}

	
			struct extent *next_extent = rb_entry(next_rb_node, struct extent, rbnode);

			// if the end of this current extent does not meet the
			// beginning of the next extent, we cannot merge them
			// so return.
			if (this_extent->last_vpn != next_extent->first_vpn - 1
			    || this_extent->last_ppn != next_extent->first_ppn - 1) {
				return;
			}

			// add all pages from the next extent to the tail of this extent
			list_splice_tail(&next_extent->page_list,
					 &this_extent->page_list);
			this_extent->page_count += next_extent->page_count;
			this_extent->last_vpn = next_extent->last_vpn;
			this_extent->last_ppn = next_extent->last_ppn;


			// remove the next extent from the tree
			rb_erase(next_rb_node, root);
			return;
		}

		// this new page cannot be added to any existing extent
		// so we now have to create a new extent and add it to the tree.

		// walk the tree to find the correct position to insert a new extent
		if (new_page->vpn < this_extent->first_vpn) {
			curr = &((*curr)->rb_left);
		} else if (new_page->vpn > this_extent->last_vpn) {
			curr = &((*curr)->rb_right);
		} 
	}

	/* Create a new extent containing only this page, and add to the tree */
	struct extent *new_ext = new_extent(my_extent_id++);
	add_page_to_extent_head(new_page, new_ext);
	rb_link_node(&new_ext->rbnode, parent, curr);
	rb_insert_color(&new_ext->rbnode, root);
}

void print_extent_tree(struct rb_root *root)
{
	// printk("printing extent tree\n");
	struct rb_node *node;
	int extents_count=0;
	int total_pages_count=0;

	for (node = rb_first(root); node; node = rb_next(node)) {
		struct extent *e = rb_entry(node, struct extent, rbnode);
		// print_extent_pg_list(e);
		extents_count++;
		total_pages_count += e->page_count;

	}
	printk(KERN_ALERT "total pages: %d, total extents: %d\n\n", total_pages_count,
	       extents_count);
}