/*
 * Implement the manual drop-all-pagecache function
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/writeback.h>
#include <linux/sysctl.h>
#include <linux/gfp.h>
#include "internal.h"

/* A global variable is a bit ugly, but it keeps the code simple */
int sysctl_drop_caches;

static void drop_pagecache_sb(struct super_block *sb, void *unused)
{
	struct inode *inode, *toput_inode = NULL;
#ifdef CONFIG_WG_PLATFORM	// WG:XD FBX-10641
	struct wg_memory_reclaim* wg_reclaim = (struct wg_memory_reclaim*) unused;
#endif

	spin_lock(&inode_sb_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		spin_lock(&inode->i_lock);
		if ((inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) ||
		    (inode->i_mapping->nrpages == 0)) {
			spin_unlock(&inode->i_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&inode_sb_list_lock);
		invalidate_mapping_pages(inode->i_mapping, 0, -1);
		iput(toput_inode);
		toput_inode = inode;

#ifdef CONFIG_WG_PLATFORM	// WG:XD FBX-10641
		/* drop_pagecache_sb() is defined as "static". so it's direct caller can only come from
		 * current .c file. We might need to do more strict validation of pointer "unused" when
		 * dereferencing it if the scope of current subroutine is changed in the future
		 */
        if (wg_reclaim && wg_reclaim->magic == WG_MEMORY_RECLAIM_MAGIC) {
			struct zoneref *z;
			struct zone* zone;
			for_each_zone_zonelist(zone, z, wg_reclaim->zonelist, gfp_zone(wg_reclaim->gfp_mask)) {
				// we may skip dropping rest page caches if we have 4 units of free pages or more
				if (zone->free_area[wg_reclaim->order].nr_free >= 4)
					break;
			}
		}
#endif
		spin_lock(&inode_sb_list_lock);
	}
	spin_unlock(&inode_sb_list_lock);
	iput(toput_inode);
}

static void drop_slab(void)
{
	int nr_objects;
	struct shrink_control shrink = {
		.gfp_mask = GFP_KERNEL,
	};

	nodes_setall(shrink.nodes_to_scan);
	do {
		nr_objects = shrink_slab(&shrink, 1000, 1000);
	} while (nr_objects > 10);
}

int drop_caches_sysctl_handler(ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (ret)
		return ret;
	if (write) {
		if (sysctl_drop_caches & 1)
			iterate_supers(drop_pagecache_sb, NULL);
		if (sysctl_drop_caches & 2)
			drop_slab();
	}
	return 0;
}

#ifdef CONFIG_WG_PLATFORM	// WG:XD FBX-10641
void wg_drop_page_caches(struct wg_memory_reclaim* wg_reclaim)
{
	iterate_supers(drop_pagecache_sb, wg_reclaim);
	return;
}
#endif
