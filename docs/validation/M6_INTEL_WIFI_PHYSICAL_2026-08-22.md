# M6 Intel Wi-Fi physical follow-up - 2026-08-22

Status: **PHYSICAL WIZARD, WPA2 ASSOCIATION, DHCP, REBOOT PERSISTENCE, CONNECTED-STATE UI, AUTHORIZATION FLOW, AND DESKTOP RADIO CONTROLS PASS**

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

## Installed helper lifecycle acceptance

Commit 09a9d66 was installed as the root-owned production helper with SHA256
32a7659cf55c9e71c64b20ca1756eae05b26da8a95fc22354a6f0199127c00d5.
The exact unprivileged fixed-word boundary returned from Wi-Fi off in 0.05
seconds and Wi-Fi on in 0.20 seconds, both below the desktop controller's 0.8
second command timeout. Off removed carrier while leaving Ethernet and its
default route intact. On raised the interface immediately, restarted WPA and
DHCP asynchronously, and restored association plus the existing DHCP lease in
two seconds. No credential, SSID, BSSID, MAC address, or lease address was
captured in the committed evidence.

This proves the installed privilege boundary and radio lifecycle on the
physical Intel adapter. The user then manually toggled Wi-Fi down and back up
from Quick Settings without the prior refusal, and enabled it from the
advanced Settings check box. A final read-only check confirmed wlan0 was up
and associated, with Ethernet still providing the preferred default route.
This closes the focused desktop-control acceptance gate on both surfaces.

## Reboot persistence acceptance

After a user-initiated reboot, a read-only check confirmed iwm0 remained the
kernel wireless parent, wlans_iwm0 recreated wlan0, and ifconfig_wlan0 retained
WPA SYNCDHCP. The interface returned up and associated with an IPv4 DHCP lease;
wpa_supplicant and the wlan0 dhclient processes were running. Ethernet
continued to own the preferred default route. The installed helper hash also
remained unchanged. No network identifiers or lease address are included in
this record.

## Wireless selection wizard acceptance

The installed candidate adds a Northstar wireless selection wizard backed by
a narrow PolicyKit action. Its protected helper discovers and persists the
wlan clone, scans without logging network identifiers, accepts a selected SSID
through a mode-0600 request containing no credential, and accepts the password
only over standard input. A native OpenSSL-backed helper derives the WPA2 PSK;
the plaintext password is neither placed in process arguments nor written to
disk. The helper preserves unrelated wpa_supplicant configuration and rolls
back a failed association.

The physical workflow passed end to end. The wizard listed nearby networks,
distinguished encryption from signal strength, accepted the administrator
authorization, derived the credential, completed WPA2 key negotiation, and
obtained a DHCP address. A FreeBSD dhclient service warning was correctly
treated as advisory after the assigned address proved success. The refreshed
list moved the active network to the top, labeled it Connected, and reported
the active connection in the status area.

ConsoleKit now registers the native SDDM session, allowing the graphical
PolicyKit agent to authorize the protected helper. The Wi-Fi window yields
while the administrator prompt is active and restores/focuses itself after
approval or cancellation, so the prompt is never hidden behind the scanner.
The user manually accepted scanning, connection, active-network reporting, and
the foreground authorization lifecycle on the physical Intel laptop.

DEV01 passed all focused Wi-Fi tests after the accepted candidate was built:

1. northstar-wificontroller;
2. northstar-wifi-configure-helper;
3. northstar-wifi-derive;
4. northstar-wifi-qml.

The accepted source head is 59ddae6. All committed and reported evidence omits
SSID, BSSID, password, derived PSK, MAC address, and lease address.

## Deferred follow-up cases

The user explicitly kept Ethernet connected during this acceptance and chose
not to make Ethernet-disconnected routing/DNS a merge gate. Wrong-password,
unavailable-network, and extended weak-signal recovery remain useful negative
hardening cases for a separate follow-up. They do not invalidate the observed
WPA2 authentication, DHCP assignment, reboot persistence, desktop controls,
or completed physical wizard workflow.

Bluetooth is separate follow-up scope. No installer, First Boot,
display/session, or USB test is reopened by this Wi-Fi work; those exact R90
physical gates remain accepted and must not be repeated.
