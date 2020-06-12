#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/error.h>

int i; // global iterator
struct tx_desc txd_arr[E1000_TXDARR_LEN] __attribute__((aligned(4096)));
struct rx_desc rxd_arr[E1000_RXDARR_LEN] __attribute__((aligned(4096)));
packet_t txd_bufs[E1000_TXDARR_LEN] __attribute__((aligned(4096)));
packet_t rxd_bufs[E1000_RXDARR_LEN] __attribute__((aligned(4096)));
// LAB 6: Your driver code here
volatile void* E1000_addr;

void tx_init(void){
    
    *(uint32_t *)(E1000_addr+E1000_TDBAL) = (uint32_t)(PADDR(txd_arr)); // Indicates start of descriptor ring buffer
    *(uint32_t *)(E1000_addr+E1000_TDBAH) = 0; // Make sure high bits are set to 0
    *(uint16_t *)(E1000_addr+E1000_TDLEN) = (uint16_t)(sizeof(struct tx_desc)*E1000_TXDARR_LEN); // Indicates length of descriptor ring buffer
    *(uint32_t *)(E1000_addr+E1000_TDH) = 0;
    *(uint32_t *)(E1000_addr+E1000_TDT) = 0;
    *(uint32_t *)(E1000_addr+E1000_TCTL) = (E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
    *(uint16_t *)(E1000_addr+E1000_TIPG) = (uint16_t)(E1000_TIPG_IPGT | E1000_TIPG_IPGR1 | E1000_TIPG_IPGR2);

    // Initialize CMD bits for transmit descriptors
    for (i = 0; i < E1000_TXDARR_LEN; i++){
        txd_arr[i].buffer_addr = (uint32_t)(PADDR(txd_bufs+i));
        txd_arr[i].cso = 0;
        txd_arr[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        txd_arr[i].status = 0;
        txd_arr[i].css = 0;
        txd_arr[i].special = 0;
        txd_arr[i].status = E1000_TXD_STAT_DD;
    }
}

int transmit(void * data_addr, uint16_t length){
    //static because the head can be in differnt location every time(cyclic buffer)
    static uint32_t next_idx = 0;
    struct tx_desc* next_ptr = (&txd_arr[next_idx]);    
    
    //if the buffer is full return error
    if (!(next_ptr->status & E1000_TXD_STAT_DD)) 
        return -1;

    //make sure that the packet does not Exceeds its max size
    length = (length > E1000_ETH_PACKET_LEN) ? E1000_ETH_PACKET_LEN: length;

    txd_arr[next_idx].buffer_addr = (uint32_t)(PADDR(txd_bufs+next_idx));
    txd_arr[next_idx].cso = 0;
    txd_arr[next_idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    txd_arr[next_idx].status = 0;
    txd_arr[next_idx].css = 0;
    txd_arr[next_idx].special = 0;
    
    next_ptr->length = length;
    memcpy((void*)(txd_bufs+next_idx), data_addr, length);

    next_idx = (next_idx+1)%E1000_TXDARR_LEN;
    *(uint32_t *)(E1000_addr+E1000_TDT) = next_idx;
    return 0;
}

void rx_init(void){
    
    *(uint32_t *)(E1000_addr+E1000_RAL) = E1000_ETH_MAC_LOW; // Set mac address for filtering
    *(uint32_t *)(E1000_addr+E1000_RAH) = E1000_ETH_MAC_HIGH;
    *(uint32_t *)(E1000_addr+E1000_RDBAL) = (uint32_t)(PADDR(rxd_arr)); // Indicates start of descriptor ring buffer
    *(uint32_t *)(E1000_addr+E1000_RDBAH) = 0; // Make sure high bits are set to 0
    *(uint16_t *)(E1000_addr+E1000_RDLEN) = (uint16_t)(sizeof(struct rx_desc)*E1000_RXDARR_LEN); // Indicates length of descriptor ring buffer
    *(uint32_t *)(E1000_addr+E1000_RDH) = 0;
    *(uint32_t *)(E1000_addr+E1000_RDT) = E1000_RXDARR_LEN;

    for (i = 0; i< E1000_RXDARR_LEN; i++)
        rxd_arr[i].buffer_addr = (uint32_t)(PADDR(rxd_bufs+i));
        rxd_arr[i].status = 0;
        rxd_arr[i].special = 0;
        
        /*
        configure the RCTL reg to allow:
        reciever enable + broadcast packet + set the HW buff to be as SW buff + strip CRC
        */
        *(uint32_t *)(E1000_addr+E1000_RCTL) = (E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE | E1000_RCTL_SECRC);
}

int recieve(void * data_addr, uint16_t* length){
    
    static uint32_t next_idx = 0;
    struct rx_desc* next_ptr = (&rxd_arr[next_idx]);
    
    // Buffer is empty
    if (!(next_ptr->status & E1000_RXD_STAT_DD))
        return -1; 

    *length = next_ptr->length;
    memcpy(data_addr, (rxd_bufs+next_idx), next_ptr->length);
    
    rxd_arr[next_idx].buffer_addr = (uint32_t)(PADDR(rxd_bufs+next_idx));
    rxd_arr[next_idx].status = 0;
    rxd_arr[next_idx].special = 0;

    *(uint32_t *)(E1000_addr+E1000_RDT) = next_idx;
    next_idx = (next_idx+1)%E1000_RXDARR_LEN;
    return 0;
}

int OSE_attach_E1000(struct pci_func *pcif)
{
    int i = 0; //general purpose to iterate
    
    pci_func_enable(pcif);
    E1000_addr = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    tx_init(); 
    rx_init();
    return 0;
}
