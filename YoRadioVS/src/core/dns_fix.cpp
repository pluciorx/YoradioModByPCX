extern "C" {
	// atrapa dla dhcps_dns_setserver_by_type
	void dhcps_dns_setserver_by_type(void* a, void* b) {
		// nic nie rób – funkcja tylko dla DHCP serwera, którego yoRadio nie używa
	}
	// atrapa dla dhcps_dns_getserver_by_type
	void dhcps_dns_getserver_by_type(void* a, void* b) {
		// nic nie rób
	}
}