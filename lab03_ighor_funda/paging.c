#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/list.h>

#include <paging.h>

struct page_list {
    atomic_t rf_count;
    struct page * m_page; 
    struct list_head list;
};

static atomic_t num_pages_allocated;
static atomic_t num_pages_freed;

static unsigned int demand_paging = 1;
module_param(demand_paging, uint, 0644);

static unsigned int
my_get_order(unsigned int value){
    unsigned int shifts = 0;
    if (!value)
        return 0;
    if (!(value & (value - 1)))
        value--;
    while (value > 0) {
        value >>= 1;
        shifts++;
    }
    return shifts;
}
static unsigned int order = 0;

static int
do_fault(struct vm_area_struct * vma,
         unsigned long           fault_address)
{
    struct page * page_ptr;
    unsigned long page_alinged_addr, pfn;
    int ret;
    struct page_list *new_data, *pages;

    printk(KERN_INFO "paging_vma_fault() invoked: took a page fault at VA 0x%lx\n", fault_address);
   
    //Allocate a new page of physical memory via the function alloc_page
    page_ptr = alloc_page(GFP_KERNEL);
    if(page_ptr == NULL){
        printk(KERN_ERR "Failed to allocate page\n");
        return VM_FAULT_OOM;
    }
    pfn = page_to_pfn(page_ptr);
    page_alinged_addr = PAGE_ALIGN(fault_address);

    //Update the process' page tables to map the faulting va to the new pa you just allocated
    ret = remap_pfn_range(vma, page_alinged_addr, pfn, PAGE_SIZE, vma->vm_page_prot);
    if (ret < 0) {
        printk(KERN_ERR "Fail to map the address area\n");
        return VM_FAULT_SIGBUS;
    }
    //remember this physical address
    new_data = (struct page_list *)kmalloc(sizeof(struct page_list), GFP_KERNEL);
    //atomic_set(&new_data->rf_count,0);
    new_data->m_page = page_ptr;
    INIT_LIST_HEAD(&new_data->list);

    pages =  vma->vm_private_data;
    list_add_tail(&(new_data->list), &(pages->list));

    atomic_add(1,&num_pages_allocated);
    
    return VM_FAULT_NOPAGE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
static int
paging_vma_fault(struct vm_area_struct * vma,
                 struct vm_fault       * vmf)
{
    unsigned long fault_address = (unsigned long)vmf->virtual_address
#else
static int
paging_vma_fault(struct vm_fault * vmf)

{
    struct vm_area_struct * vma = vmf->vma;
    unsigned long fault_address = (unsigned long)vmf->address;
#endif

    return do_fault(vma, fault_address);
}

static void
paging_vma_open(struct vm_area_struct * vma)
{
    struct page_list *pages;
    printk(KERN_INFO "paging_vma_open() invoked\n");

    //retrieve a pointer to our new data structure
    pages = vma->vm_private_data;
    //increment the reference count of its atomic variable
    atomic_add(1,&pages->rf_count);

}

static void
paging_vma_close(struct vm_area_struct * vma)
{
    struct page_list *pages,*tmp;
    struct list_head *pos, *q;
    printk(KERN_INFO "paging_vma_close() invoked\n");

    //printk("traversing the list, head's rf_count %d\n", atomic_read(&pages->rf_count));
    //list_for_each_entry(tmp, &pages->list, list){
    //     printk("page VA.start 0x%lx PA 0x%lx rf_count %d\n", vma->vm_start, PFN_PHYS(page_to_pfn(tmp->m_page)), tmp->rf_count );
    //}
    
    //retrieve your data structure
    pages = vma->vm_private_data;
    //decrement its reference count
    atomic_sub(1,&pages->rf_count);
    //if it becomes zero, free the dynamically allocated memory
    if(atomic_read(&pages->rf_count) == 0){//demand-paging
        //Free physical memory allocated previously
        //Free our tracker structure and any other dynamically allocated memory
        list_for_each_safe(pos, q, &pages->list){
            tmp= list_entry(pos, struct page_list, list);
             __free_page(tmp->m_page);
            atomic_add(1,&num_pages_freed);
            list_del(pos);
            kfree(tmp);
        }
    }
    if(pages->m_page != NULL){//pre-paging
        __free_pages(tmp->m_page,order);
        atomic_add(1,&num_pages_freed);
    }
    kfree(pages);
}

static struct vm_operations_struct
paging_vma_ops = 
{
    .fault = paging_vma_fault,
    .open  = paging_vma_open,
    .close = paging_vma_close
};

/* vma is the new virtual address segment for the process */
static int
paging_mmap(struct file           * filp,
            struct vm_area_struct * vma)
{
    struct page_list *plist;

    //variables for pre paging
    long unsigned int num_pages, len;
    struct page *pages;
    unsigned long pfn, page_alinged_addr;
    int ret;

    /* prevent Linux from mucking with our VMA (expanding it, merging it 
     * with other VMAs, etc.)
     */
    vma->vm_flags |= VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_NORESERVE
              | VM_DONTDUMP | VM_PFNMAP;

    /* setup the vma->vm_ops, so we can catch page faults */
    vma->vm_ops = &paging_vma_ops;

    printk(KERN_INFO "paging_mmap() invoked: new VMA for pid %d from VA 0x%lx to 0x%lx\n",
        current->pid, vma->vm_start, vma->vm_end);
    
    //PRE PAGING
    if(demand_paging == 0){
        len = vma->vm_end - vma->vm_start;
        num_pages = len / PAGE_SIZE;
        if(len % PAGE_SIZE){
            num_pages++;
        }
        //printk("Number of pages needs to be allocated %lu\n",num_pages);

        order = my_get_order(num_pages);
        pages = alloc_pages(GFP_KERNEL, order);
        if(pages == NULL){
            printk(KERN_ERR "Failed to allocate page\n");
            return -ENOMEM;
        }
        pfn = page_to_pfn(pages);
        page_alinged_addr = PAGE_ALIGN(vma->vm_start);

        ret = remap_pfn_range(vma, page_alinged_addr, pfn, PAGE_SIZE*num_pages, vma->vm_page_prot);
        if (ret < 0) {
            printk(KERN_ERR "Fail to map the address area\n");
            return -EFAULT;
        }
    }

    //New data structure
    plist = (struct page_list *)kmalloc(sizeof(struct page_list), GFP_KERNEL);
    atomic_set(&plist->rf_count,1);
    plist->m_page = NULL;
    if(!demand_paging){
        //if pre paging, then remember this physical address
        plist->m_page = pages;
        atomic_add(1,&num_pages_allocated);
    }
    INIT_LIST_HEAD(&(plist->list));
    vma->vm_private_data = (void *)plist;

    return 0;
}

static struct file_operations
dev_ops =
{
    .mmap = paging_mmap,
};

static struct miscdevice
dev_handle =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = PAGING_MODULE_NAME,
    .fops = &dev_ops,
};
/*** END device I/O **/

/*** Kernel module initialization and teardown ***/
static int
kmod_paging_init(void)
{
    int status;

    /* Create a character device to communicate with user-space via file I/O operations */
    status = misc_register(&dev_handle);
    if (status != 0) {
        printk(KERN_ERR "Failed to register misc. device for module\n");
        return status;
    }

    printk(KERN_INFO "Loaded kmod_paging module\n");
    atomic_set(&num_pages_allocated,0);
    atomic_set(&num_pages_freed,0);

    return 0;
}

static void
kmod_paging_exit(void)
{
    /* Deregister our device file */
    misc_deregister(&dev_handle);

    printk("Number of pages allocated/freed: %d/%d",atomic_read(&num_pages_allocated),atomic_read(&num_pages_freed));

    printk(KERN_INFO "Unloaded kmod_paging module\n");
}

module_init(kmod_paging_init);
module_exit(kmod_paging_exit);

/* Misc module info */
MODULE_LICENSE("GPL");
