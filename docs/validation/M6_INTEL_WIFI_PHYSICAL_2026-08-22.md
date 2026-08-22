# M6 Intel Wi-Fi physical follow-up - 2026-08-22

Status: **PHYSICAL ASSOCIATION AND DHCP PASS - desktop detection passes; exact radio-control authorization fix is locally tested and physical toggle acceptance remains pending**

This follow-up starts from merged Intel Alpha PR #116 at
c20e3159f8cbc630abbb3e7e22e647242df51430. It records only privacy-bounded
evidence: the home SSID, BSSID, passphrase, derived PSK, MAC address, and lease
address are deliberately omitted.

## Installed hardware and runtime

The physical Whiskey Lake laptop exposes an Intel Dual Band Wireless-AC 9560
parent as iwm0. The integrated R90 install contains
wifi-firmware-iwlwifi-kmod-9000-20260410, and if_iwm.ko is loaded. Kernel
messages identify the adapter and a loaded firmware revision.

Immediately after installation, net.wlan.devices reported iwm0, but
ifconfig -l contained only wired Ethernet and loopback. No cloned wlan
interface, wlans_iwm0, or ifconfig_wlan0 configuration existed. This means the
current desktop Wi-Fi controls cannot discover the adapter until an
administrator manually creates the clone.

## Physical configuration evidence

The administrator created wlan0 with parent iwm0 and persisted:

~~~text
wlans_iwm0=wlan0
ifconfig_wlan0=WPA SYNCDHCP
~~~

The adapter scanned both 2.4 GHz and non-DFS 5 GHz channels and observed 71
access points. The selected privacy-redacted WPA2 network was visible on
2.4 GHz, although its scan sample reported a very weak signal.

The installed bsdconfig wireless path did not complete configuration. It first
reported no wireless device before wlan0 existed, then attempted to edit an
empty SSID and displayed:

~~~text
Cannot edit wireless configuration; no matches for an empty SSID in
wpa_supplicant.conf(5)
~~~

A credential-safe recovery generated a hashed WPA PSK into a root-only
/etc/wpa_supplicant.conf; no plaintext credential was committed or included in
this evidence. The first synchronous DHCP start raced association and printed
a timeout warning. Association completed immediately afterward, the interface
reported status: associated with WPA2 privacy enabled, DHCP assigned an IPv4
lease, and three LAN probes to the wireless address passed with zero loss.

Wired Ethernet remained connected throughout. Its default route remained
preferred, which is expected and allows Wi-Fi configuration without disrupting
the active SSH/control path. The successful wireless DHCP exchange and direct
LAN reachability prove the Wi-Fi data path independently of the wired default
route.

## Desktop control evidence and refusal

With both Ethernet and Wi-Fi active, Quick Settings reported the redacted
network as connected and Settings exposed Wi-Fi as an available checked
control. This closes real-hardware detection, the FreeBSD associated-status
interpretation, and agreement between the two surfaces.

Clicking either control was refused. Read-only inspection showed that the
root-owned installed northstar-radio helper ran sudo -n for raw ifconfig, while
the production administrator had only password-required general sudo. The
failure is therefore the previously documented unfinished production privilege
boundary, not a radio, association, or controller-state failure.

The follow-up changes the fixed helper to validate wifi|bluetooth plus on|off
before re-entering the exact root-owned installed helper through sudo. First
Boot adds NOPASSWD authorization for only the four exact helper invocations;
it does not authorize raw ifconfig, service, arbitrary helper arguments, or
general passwordless sudo. Isolated boundary and First Boot provisioning tests
cover the fixed-word validation, pre-sudo refusal, root mutation path, absent
hardware status, exact generated policy, ownership mode, and rollback behavior.
## Product gaps and remaining acceptance

The image supplies the correct Intel firmware but does not yet turn a detected
net.wlan.devices parent into a persistent wlan interface. First Boot also has
no credential-safe network selection flow, and the base-system bsdconfig
wireless behavior observed above is not an acceptable Northstar setup
experience.

The follow-up must keep credentials out of logs, process evidence, source, and
validation records while it addresses or documents these boundaries. Before
promotion, complete:

1. reboot persistence: wlan0 is recreated, associates, and receives DHCP;
2. dual-interface behavior with Ethernet still connected and preferred;
3. Ethernet-disconnected Wi-Fi routing and DNS without manual repair;
4. Quick Settings and Settings status for the real associated adapter;
5. Wi-Fi off/on through the installed fixed-argument privilege boundary; and
6. recovery behavior for weak signal, wrong credentials, and unavailable
   networks without exposing the passphrase.

No installer, First Boot, display/session, or USB test is reopened by this
follow-up. Those exact R90 physical gates remain accepted and must not be
repeated.
