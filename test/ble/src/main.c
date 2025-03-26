#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/settings/settings.h>

size_t num_ids = 0;

void print_ids(void)
{
  char addr_str[BT_ADDR_LE_STR_LEN];

  bt_id_get(NULL, &num_ids);
  printk("Num IDs: %d\n", num_ids);
  bt_addr_le_t addrs[num_ids];
  
  for (int i = 0; i < num_ids; i++) {
    bt_id_get(addrs, &num_ids);
    bt_addr_le_to_str(&addrs[i], addr_str, sizeof(addr_str));
    printk("Addr[%d]: %s\n", i, addr_str);
  }
}

int main(void)
{
  print_ids();

	int err = bt_enable(NULL);
	if (err) printk("Bluetooth init failed (err %d)\n", err);

  err = settings_load();
  if (err) printk("settings_load failed (err %d)\n", err);

  print_ids();

  if (num_ids < CONFIG_BT_ID_MAX) {
    bt_addr_le_t new_addr;

    err = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &new_addr);
    if (err) printk("Invalid BT address (err %d)\n", err);

    err = bt_id_create(&new_addr, NULL);
    if (err) printk("Creating new ID failed (err %d)\n", err);

    print_ids();
  }
	
  return 0;
}
