#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/tcp.h>
#include <net/if_inet6.h>
#include <net/addrconf.h>

struct dev_addr_info {
	struct list_head list;
	struct net_device *dev;
	int family;
	union {
		__be32 addr4;
		struct in6_addr addr6;
	} addr;
};

static struct list_head dev_info;

static struct dev_addr_info *get_dev_info(struct list_head *head,
					  struct net_device *dev, int family)
{
	struct dev_addr_info *info;

	list_for_each_entry(info, head, list) {
		if (!strcmp(info->dev->name, dev->name) &&
		    info->family == family)
			return info;
	}

	return NULL;
}

static void del_dev_info(struct list_head *head, struct net_device *dev)
{
	struct dev_addr_info *info, *temp;

	list_for_each_entry_safe(info, temp, head, list) {
		if (!strcmp(info->dev->name, dev->name)) {
			list_del(&info->list);
			kfree(info);
		}
	}
}

void dev_monitor_cleanup_sock(struct net_device *dev)
{
	struct sock *sk;
	const struct hlist_nulls_node *node;
	struct inet_ehash_bucket *head;
	unsigned int slot;
	struct dev_addr_info *addr_info, *addr6_info;
	__be32 addr4;
	struct in6_addr addr6;

	pr_err("%s(), %s\n", __func__, dev->name);
	/* better to merge AF_INET addr info and AF_INET6 addr info */
	addr_info = get_dev_info(&dev_info, dev, AF_INET);
	if (addr_info)
		addr4 = addr_info->addr.addr4;

	addr6_info = get_dev_info(&dev_info, dev, AF_INET6);
	if (addr6_info)
		addr6 = addr6_info->addr.addr6;

	if (!addr_info && !addr6_info)
		return;

	for (slot = 0; slot <= tcp_hashinfo.ehash_mask; slot++) {
		head = &tcp_hashinfo.ehash[slot];
		sk_nulls_for_each_rcu(sk, node, &head->chain) {
			/*
			pr_err("%p, family = %d, sk state = %d\n", sk, sk->sk_family, sk->sk_state);
			if (sk->sk_family == AF_INET)
				pr_err("%pI4 %pI4\n", &sk->sk_rcv_saddr, &sk->sk_daddr);
			else
				pr_err("%pI6 %pI6\n", &sk->sk_v6_rcv_saddr, &sk->sk_v6_daddr);
			*/

			if (addr_info && sk->sk_family == AF_INET) {
				if (addr4 == sk->sk_rcv_saddr) {
					if (sk->sk_state == TCP_FIN_WAIT1 ||
					    sk->sk_state == TCP_ESTABLISHED)
						sk->sk_prot->disconnect(sk, 0);
				}
			}

			if (addr6_info && sk->sk_family == AF_INET6) {
				if (ipv6_addr_equal(&addr6, &sk->sk_v6_rcv_saddr)) {
					if (sk->sk_state == TCP_FIN_WAIT1 ||
					    sk->sk_state == TCP_ESTABLISHED)
						sk->sk_prot->disconnect(sk, 0);
				}
			}
		}
	}
}

static int dev_monitor_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *info) {
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	pr_err("dev : %s : event : %d\n", dev->name, event);
	switch (event) {
//	case NETDEV_DOWN:
	case NETDEV_UNREGISTER_FINAL:
		/* for given net device, address matched sock will be closed */
		dev_monitor_cleanup_sock(dev);
		/* remove dev info from the list */
		del_dev_info(&dev_info, dev);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_nb = {
	.notifier_call = dev_monitor_notifier_cb,
};

static int dev_monitor_inetaddr_event(struct notifier_block *this,
				      unsigned long event, void *ptr)
{
	const struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *dev;
	struct dev_addr_info *addr_info;

	if (!ifa || !ifa->ifa_dev)
		return NOTIFY_DONE;


	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGE:
	case NETDEV_CHANGEADDR:

		dev = ifa->ifa_dev->dev;

		pr_err("%s %s: %pI4\n", __func__, dev->name, &ifa->ifa_local);

		addr_info = get_dev_info(&dev_info, dev, AF_INET);
		if (!addr_info) {
			addr_info = kzalloc(sizeof(struct dev_addr_info),
					    GFP_ATOMIC);
			if (!addr_info)
				break;
			addr_info->dev = dev;
			addr_info->family = AF_INET;
			addr_info->addr.addr4 = ifa->ifa_local;

			list_add(&addr_info->list, &dev_info);
		} else {
			addr_info->dev = dev;
			addr_info->family = AF_INET;
			addr_info->addr.addr4 = ifa->ifa_local;
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_inetaddr_nb = {
	.notifier_call = dev_monitor_inetaddr_event,
};

static int dev_monitor_inet6addr_event(struct notifier_block *this,
				      unsigned long event, void *ptr)
{
	const struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *dev;
	struct dev_addr_info *addr_info;
	int addr_type;

	if (!ifa || !ifa->idev)
		return NOTIFY_DONE;

	addr_type = ipv6_addr_type(&ifa->addr);

	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGE:
	case NETDEV_CHANGEADDR:
		if (ifa->scope > RT_SCOPE_LINK ||
		    addr_type == IPV6_ADDR_ANY ||
		    (addr_type & IPV6_ADDR_LOOPBACK) ||
		    (addr_type & IPV6_ADDR_LINKLOCAL))
			break;

		dev = ifa->idev->dev;

		pr_err("%s %s: %pI6\n", __func__, dev->name, &ifa->addr);

		addr_info = get_dev_info(&dev_info, dev, AF_INET6);
		if (!addr_info) {
			addr_info = kzalloc(sizeof(struct dev_addr_info),
					    GFP_ATOMIC);
			if (!addr_info)
				break;
			addr_info->dev = dev;
			addr_info->family = AF_INET6;
			addr_info->addr.addr6 = ifa->addr;

			INIT_LIST_HEAD(&addr_info->list);
			list_add(&addr_info->list, &dev_info);
		} else {
			addr_info->dev = dev;
			addr_info->family = AF_INET6;
			addr_info->addr.addr6 = ifa->addr;
		}

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_inet6addr_nb = {
	.notifier_call = dev_monitor_inet6addr_event,
};

static int __init dev_monitor_init(void)
{
	int ret;

	INIT_LIST_HEAD(&dev_info);
	ret = register_netdevice_notifier(&dev_monitor_nb);
	if (ret) {
		pr_err("%s: registering notifier error %d\n", __func__, ret);
	}
	register_inetaddr_notifier(&dev_monitor_inetaddr_nb);
	register_inet6addr_notifier(&dev_monitor_inet6addr_nb);
	return ret;
}

static void __exit dev_monitor_exit(void)
{
	unregister_netdevice_notifier(&dev_monitor_nb);
}

module_init(dev_monitor_init);
module_exit(dev_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter J. Park <gyujoon.park@samsung.com>");
