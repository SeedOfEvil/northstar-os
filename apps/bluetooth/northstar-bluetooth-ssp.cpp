/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Northstar's narrow Secure Simple Pairing agent for FreeBSD raw HCI.
 * hcsecd remains responsible for link-key lookup and persistence; this
 * process handles only the SSP events that hcsecd does not understand.
 */

#include <sys/endian.h>
#include <poll.h>

#define L2CAP_SOCKET_CHECKED
#include <bluetooth.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {
constexpr int ExitUsage = 64;
constexpr int ExitData = 65;
constexpr int ExitUnavailable = 69;
constexpr int PairingTimeoutSeconds = 90;
constexpr uint8_t IoCapabilityDisplayYesNo = 0x01;
constexpr uint8_t AuthenticationGeneralBondingMitm = 0x05;
constexpr uint16_t ReadSimplePairingMode = 0x0055;
constexpr uint8_t RequestFailed = 0xff;
constexpr uint8_t IoCapabilityResponseEvent = 0x32;

struct SimplePairingModeReply {
    uint8_t status;
    uint8_t mode;
} __attribute__((packed));

struct IoCapabilityResponseEventParameters {
    bdaddr_t bdaddr;
    uint8_t ioCapability;
    uint8_t oobDataPresent;
    uint8_t authenticationRequirements;
} __attribute__((packed));

uint8_t requestCommand(int socket, uint16_t opcode, void *parameters,
                       size_t length);

uint8_t readScanEnable(int socket, uint8_t *scanEnable)
{
    ng_hci_read_scan_enable_rp reply{};
    struct bt_devreq request{};
    request.opcode = NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND,
                                   NG_HCI_OCF_READ_SCAN_ENABLE);
    request.event = NG_HCI_EVENT_COMMAND_COMPL;
    request.rparam = &reply;
    request.rlen = sizeof(reply);
    if (bt_devreq(socket, &request, 5) != 0)
        return RequestFailed;
    if (reply.status == 0) *scanEnable = reply.scan_enable;
    return reply.status;
}

uint8_t writeScanEnable(int socket, uint8_t scanEnable)
{
    ng_hci_write_scan_enable_cp parameter{scanEnable};
    return requestCommand(socket,
        NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND,
                      NG_HCI_OCF_WRITE_SCAN_ENABLE),
        &parameter, sizeof(parameter));
}

bool sameAddress(const bdaddr_t &left, const bdaddr_t &right)
{
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}

int fail(int status, const char *message)
{
    std::fprintf(stderr, "ERROR: %s\n", message);
    return status;
}

uint8_t requestCommand(int socket, uint16_t opcode, void *parameters, size_t length)
{
    ng_hci_status_rp reply{};
    struct bt_devreq request{};
    request.opcode = opcode;
    request.event = NG_HCI_EVENT_COMMAND_COMPL;
    request.cparam = parameters;
    request.clen = length;
    request.rparam = &reply;
    request.rlen = sizeof(reply);
    return bt_devreq(socket, &request, 5) == 0 ? reply.status : RequestFailed;
}

uint8_t readSimplePairingMode(int socket, uint8_t *mode)
{
    SimplePairingModeReply reply{};
    struct bt_devreq request{};
    request.opcode = NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND, ReadSimplePairingMode);
    request.event = NG_HCI_EVENT_COMMAND_COMPL;
    request.rparam = &reply;
    request.rlen = sizeof(reply);
    if (bt_devreq(socket, &request, 5) != 0)
        return RequestFailed;
    if (reply.status == 0) *mode = reply.mode;
    return reply.status;
}

bool sendCommand(int socket, uint16_t opcode, void *parameters, size_t length)
{
    return bt_devsend(socket, opcode, parameters, length) == 0;
}

bool readDecision(bool *accepted)
{
    pollfd input{STDIN_FILENO, POLLIN, 0};
    int result;
    do {
        result = poll(&input, 1, PairingTimeoutSeconds * 1000);
    } while (result < 0 && errno == EINTR);
    if (result <= 0 || (input.revents & (POLLIN | POLLHUP)) == 0)
        return false;

    char response[16]{};
    if (std::fgets(response, sizeof(response), stdin) == nullptr)
        return false;
    const std::string value(response);
    if (value == "accept\n" || value == "accept") {
        *accepted = true;
        return true;
    }
    if (value == "reject\n" || value == "reject") {
        *accepted = false;
        return true;
    }
    return false;
}

int selfTest()
{
    bdaddr_t address{};
    if (bt_aton("aa:bb:cc:dd:ee:ff", &address) != 1)
        return fail(1, "Bluetooth address parser self-test failed");
    if (IoCapabilityDisplayYesNo != 1 || AuthenticationGeneralBondingMitm != 5)
        return fail(1, "SSP capability self-test failed");
    std::puts("PASS: Northstar Bluetooth SSP agent protocol");
    return 0;
}

bool validNode(const std::string &node)
{
    if (node.size() < 7 || node.size() > 15 || node.rfind("ubt", 0) != 0
        || node.substr(node.size() - 3) != "hci")
        return false;
    for (size_t i = 3; i + 3 < node.size(); ++i) {
        if (node[i] < '0' || node[i] > '9') return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0)
        return selfTest();
    const bool probe = argc == 3 && std::strcmp(argv[1], "--probe") == 0;
    const bool enableProbe = argc == 3
        && std::strcmp(argv[1], "--enable-probe") == 0;
    const bool listen = argc == 4 && std::strcmp(argv[1], "--listen") == 0;
    if ((!listen && argc != 3) || (listen && argc != 4))
        return fail(ExitUsage,
                    "usage: northstar-bluetooth-ssp NODE BD_ADDR | --listen NODE BD_ADDR | --probe NODE | --enable-probe NODE");
    if (geteuid() != 0)
        return fail(77, "the SSP pairing agent requires root");

    const std::string node((probe || enableProbe || listen) ? argv[2] : argv[1]);
    if (!validNode(node))
        return fail(ExitData, "the Bluetooth controller name is invalid");

    bdaddr_t target{};
    const char *targetText = listen ? argv[3] : argv[2];
    if (!probe && !enableProbe && bt_aton(targetText, &target) != 1)
        return fail(ExitData, "the Bluetooth address is invalid");

    const int socket = bt_devopen(node.c_str());
    if (socket < 0)
        return fail(ExitUnavailable, "the Bluetooth controller could not be opened");

    uint8_t simplePairingMode = 0;
    const uint8_t readPairingStatus = readSimplePairingMode(socket, &simplePairingMode);
    if (probe || enableProbe) {
        if (readPairingStatus != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "Simple Pairing Mode could not be read");
        }
        std::printf("NORTHSTAR_BLUETOOTH_SSP_MODE=%s\n",
                    simplePairingMode == 1 ? "enabled" : "disabled");
        if (enableProbe && simplePairingMode != 1) {
            uint8_t scanEnable = 0;
            const uint8_t readScanStatus = readScanEnable(socket, &scanEnable);
            std::printf("NORTHSTAR_BLUETOOTH_SCAN_MODE=0x%02x\n", scanEnable);
            if (readScanStatus != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "Bluetooth scan mode could not be read");
            }
            if (scanEnable != 0) {
                const uint8_t disableStatus = writeScanEnable(socket, 0);
                std::printf("NORTHSTAR_BLUETOOTH_SCAN_DISABLE_STATUS=0x%02x\n",
                            disableStatus);
                if (disableStatus != 0) {
                    bt_devclose(socket);
                    return fail(ExitUnavailable,
                                "Bluetooth scanning could not be paused");
                }
            }
            ng_hci_write_simple_pairing_cp simplePairing{1};
            const uint8_t writeStatus = requestCommand(socket,
                NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND,
                              NG_HCI_OCF_WRITE_SIMPLE_PAIRING),
                &simplePairing, sizeof(simplePairing));
            std::printf("NORTHSTAR_BLUETOOTH_SSP_WRITE_STATUS=0x%02x\n",
                        writeStatus);
            if (writeStatus != 0) {
                if (scanEnable != 0) (void)writeScanEnable(socket, scanEnable);
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "Secure Simple Pairing could not be enabled");
            }
            const uint8_t verifyStatus = readSimplePairingMode(
                socket, &simplePairingMode);
            if (verifyStatus != 0 || simplePairingMode != 1) {
                if (scanEnable != 0) (void)writeScanEnable(socket, scanEnable);
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "Secure Simple Pairing enable did not persist");
            }
            std::puts("NORTHSTAR_BLUETOOTH_SSP_MODE=enabled");
            if (scanEnable != 0) {
                const uint8_t restoreStatus = writeScanEnable(socket, scanEnable);
                std::printf("NORTHSTAR_BLUETOOTH_SCAN_RESTORE_STATUS=0x%02x\n",
                            restoreStatus);
                if (restoreStatus != 0) {
                    bt_devclose(socket);
                    return fail(ExitUnavailable,
                                "Bluetooth scanning could not be restored");
                }
            }
        }
        bt_devclose(socket);
        return simplePairingMode <= 1 ? 0
                                      : fail(ExitUnavailable,
                                             "the controller returned an invalid SSP mode");
    }

    if (readPairingStatus != 0 || simplePairingMode != 1) {
        uint8_t scanEnable = 0;
        if (readScanEnable(socket, &scanEnable) != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "Bluetooth scan mode could not be read");
        }
        if (scanEnable != 0 && writeScanEnable(socket, 0) != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "Bluetooth scanning could not be paused");
        }

        ng_hci_write_simple_pairing_cp simplePairing{1};
        const uint8_t writeStatus = requestCommand(socket,
                NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND,
                              NG_HCI_OCF_WRITE_SIMPLE_PAIRING),
                &simplePairing, sizeof(simplePairing));
        if (writeStatus != 0) {
            if (scanEnable != 0) (void)writeScanEnable(socket, scanEnable);
            bt_devclose(socket);
            return fail(ExitUnavailable, "Secure Simple Pairing could not be enabled");
        }
        if (scanEnable != 0 && writeScanEnable(socket, scanEnable) != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "Bluetooth scanning could not be restored");
        }
    }

    ng_hci_set_event_mask_cp eventMask{};
    uint64_t enabledEvents = NG_HCI_EVENT_MASK_DEFAULT
        | NG_HCI_EVMSK_IO_CAPABILITY_REQ
        | NG_HCI_EVMSK_IO_CAPABILITY_RESP
        | NG_HCI_EVMSK_USER_CONFIRMATION_REQ
        | NG_HCI_EVMSK_USER_PASSKEY_REQ
        | NG_HCI_EVMSK_SIMPLE_PAIRING_COMPL
        | NG_HCI_EVMSK_USER_PASSKEY_NOTIFICATION;
    enabledEvents = htole64(enabledEvents);
    std::memcpy(eventMask.event_mask, &enabledEvents, sizeof(enabledEvents));
    if (requestCommand(socket,
            NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND, NG_HCI_OCF_SET_EVENT_MASK),
            &eventMask, sizeof(eventMask)) != 0) {
        bt_devclose(socket);
        return fail(ExitUnavailable, "Secure Simple Pairing events could not be enabled");
    }

    ng_hci_write_auth_enable_cp authentication{1};
    if (requestCommand(socket,
            NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND, NG_HCI_OCF_WRITE_AUTH_ENABLE),
            &authentication, sizeof(authentication)) != 0) {
        bt_devclose(socket);
        return fail(ExitUnavailable, "Bluetooth authentication could not be enabled");
    }

    struct bt_devfilter filter{};
    bt_devfilter_pkt_set(&filter, NG_HCI_EVENT_PKT);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_COMMAND_COMPL);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_COMMAND_STATUS);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_CON_COMPL);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_AUTH_COMPL);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_PIN_CODE_REQ);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_LINK_KEY_REQ);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_LINK_KEY_NOTIFICATION);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_IO_CAPABILITY_REQUEST);
    bt_devfilter_evt_set(&filter, IoCapabilityResponseEvent);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_USER_CONFIRMATION_REQUEST);
    bt_devfilter_evt_set(&filter, NG_HCI_EVENT_SIMPLE_PAIRING_COMPLETE);
    if (bt_devfilter(socket, &filter, nullptr) < 0) {
        bt_devclose(socket);
        return fail(ExitUnavailable, "the SSP event filter could not be installed");
    }

    uint8_t priorScanEnable = 0;
    if (listen) {
        if (readScanEnable(socket, &priorScanEnable) != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable,
                        "Bluetooth discoverability state could not be read");
        }
        if (writeScanEnable(socket, 3) != 0) {
            bt_devclose(socket);
            return fail(ExitUnavailable,
                        "Bluetooth discoverability could not be enabled");
        }
        std::puts("NORTHSTAR_BLUETOOTH_INBOUND_PAIRING=WAITING");
        std::fflush(stdout);
    } else {
        ng_hci_create_con_cp connection{};
        connection.bdaddr = target;
        connection.pkt_type = htole16(0xcc18);
        connection.page_scan_rep_mode = 1;
        connection.page_scan_mode = 0;
        connection.clock_offset = 0;
        connection.accept_role_switch = 1;
        if (!sendCommand(socket,
                NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL, NG_HCI_OCF_CREATE_CON),
                &connection, sizeof(connection))) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "the Bluetooth connection could not be initiated");
        }
    }

    unsigned char buffer[NG_HCI_EVENT_PKT_SIZE + sizeof(ng_hci_event_pkt_t)]{};
    bool confirmationAnswered = false;
    bool pairingComplete = false;
    bool authenticationComplete = false;
    bool capabilityReplied = false;
    uint16_t targetHandle = 0xffff;
    for (;;) {
        const ssize_t count = bt_devrecv(socket, buffer, sizeof(buffer), PairingTimeoutSeconds);
        if (count < static_cast<ssize_t>(sizeof(ng_hci_event_pkt_t))) {
            bt_devclose(socket);
            return fail(ExitUnavailable, "the device did not complete Bluetooth pairing");
        }
        const auto *event = reinterpret_cast<const ng_hci_event_pkt_t *>(buffer);
        if (event->type != NG_HCI_EVENT_PKT
            || count < static_cast<ssize_t>(sizeof(*event) + event->length))
            continue;
        const unsigned char *payload = buffer + sizeof(*event);

        if (event->event == NG_HCI_EVENT_COMMAND_COMPL
            && event->length >= sizeof(ng_hci_command_compl_ep) + 1) {
            const auto *complete =
                reinterpret_cast<const ng_hci_command_compl_ep *>(payload);
            std::printf("NORTHSTAR_BLUETOOTH_HCI_COMMAND_COMPLETE=0x%04x:0x%02x\n",
                        le16toh(complete->opcode),
                        payload[sizeof(ng_hci_command_compl_ep)]);
            std::fflush(stdout);
            continue;
        }

        if (event->event == NG_HCI_EVENT_COMMAND_STATUS
            && event->length >= sizeof(ng_hci_command_status_ep)) {
            const auto *status =
                reinterpret_cast<const ng_hci_command_status_ep *>(payload);
            const uint16_t opcode = le16toh(status->opcode);
            std::printf("NORTHSTAR_BLUETOOTH_HCI_COMMAND_STATUS=0x%04x:0x%02x\n",
                        opcode, status->status);
            std::fflush(stdout);
            if (opcode == NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
                                        NG_HCI_OCF_CREATE_CON)
                && status->status != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "the Bluetooth connection command was rejected");
            }
            if (opcode == NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
                                        NG_HCI_OCF_AUTH_REQ)
                && status->status != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "the Bluetooth authentication command was rejected");
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_CON_COMPL
            && event->length >= sizeof(ng_hci_con_compl_ep)) {
            const auto *complete = reinterpret_cast<const ng_hci_con_compl_ep *>(payload);
            if (sameAddress(complete->bdaddr, target)) {
                std::printf("NORTHSTAR_BLUETOOTH_HCI_CONNECTION_STATUS=0x%02x\n",
                            complete->status);
                std::fflush(stdout);
            }
            if (sameAddress(complete->bdaddr, target) && complete->status != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable, "the selected device rejected the Bluetooth connection");
            }
            if (sameAddress(complete->bdaddr, target) && !listen) {
                targetHandle = complete->con_handle;
                ng_hci_auth_req_cp authenticationRequest{};
                authenticationRequest.con_handle = complete->con_handle;
                if (!sendCommand(socket,
                        NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
                                      NG_HCI_OCF_AUTH_REQ),
                        &authenticationRequest, sizeof(authenticationRequest))) {
                    bt_devclose(socket);
                    return fail(ExitUnavailable,
                                "Bluetooth authentication could not be requested");
                }
            }
            if (sameAddress(complete->bdaddr, target) && listen)
                targetHandle = complete->con_handle;
            continue;
        }

        if (event->event == NG_HCI_EVENT_AUTH_COMPL
            && event->length >= sizeof(ng_hci_auth_compl_ep)) {
            const auto *complete =
                reinterpret_cast<const ng_hci_auth_compl_ep *>(payload);
            if (complete->con_handle != targetHandle)
                continue;
            std::printf("NORTHSTAR_BLUETOOTH_HCI_AUTHENTICATION_STATUS=0x%02x\n",
                        complete->status);
            std::fflush(stdout);
            if (complete->status != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "Bluetooth authentication did not complete");
            }
            authenticationComplete = true;
            if (pairingComplete) {
                std::puts(confirmationAnswered
                    ? "NORTHSTAR_BLUETOOTH_PAIRED=CONFIRMED"
                    : "NORTHSTAR_BLUETOOTH_PAIRED=JUST_WORKS");
                std::fflush(stdout);
                bt_devclose(socket);
                return 0;
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_LINK_KEY_REQ
            && event->length >= sizeof(bdaddr_t)) {
            const auto *address = reinterpret_cast<const bdaddr_t *>(payload);
            if (sameAddress(*address, target)) {
                std::puts("NORTHSTAR_BLUETOOTH_HCI_LINK_KEY_REQUEST=1");
                std::fflush(stdout);
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_LINK_KEY_NOTIFICATION
            && event->length >= sizeof(ng_hci_link_key_notification_ep)) {
            const auto *notification =
                reinterpret_cast<const ng_hci_link_key_notification_ep *>(payload);
            if (!sameAddress(notification->bdaddr, target))
                continue;
            std::puts("NORTHSTAR_BLUETOOTH_HCI_LINK_KEY_NOTIFICATION=1");
            std::puts(confirmationAnswered
                ? "NORTHSTAR_BLUETOOTH_PAIRED=CONFIRMED"
                : "NORTHSTAR_BLUETOOTH_PAIRED=JUST_WORKS");
            std::fflush(stdout);
            if (listen && writeScanEnable(socket, priorScanEnable) != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable,
                            "Bluetooth discoverability could not be restored");
            }
            bt_devclose(socket);
            return 0;
        }

        if (event->event == NG_HCI_EVENT_PIN_CODE_REQ
            && event->length >= sizeof(bdaddr_t)) {
            const auto *address = reinterpret_cast<const bdaddr_t *>(payload);
            if (sameAddress(*address, target)) {
                std::puts("NORTHSTAR_BLUETOOTH_HCI_PIN_CODE_REQUEST=1");
                std::fflush(stdout);
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_IO_CAPABILITY_REQUEST
            && event->length >= sizeof(ng_hci_io_capability_request_ep)) {
            const auto *request = reinterpret_cast<const ng_hci_io_capability_request_ep *>(payload);
            if (!sameAddress(request->bdaddr, target))
                continue;
            std::puts("NORTHSTAR_BLUETOOTH_HCI_IO_CAPABILITY_REQUEST=1");
            std::fflush(stdout);
            ng_hci_io_capability_request_reply_cp reply{};
            reply.bdaddr = target;
            reply.io_capability = IoCapabilityDisplayYesNo;
            reply.oob_data_present = 0;
            reply.authentication_requirements = AuthenticationGeneralBondingMitm;
            if (!sendCommand(socket,
                    NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
                                  NG_HCI_IO_CAPABILITY_REQUEST_REPLY),
                    &reply, sizeof(reply))) {
                bt_devclose(socket);
                return fail(ExitUnavailable, "the SSP capability reply failed");
            }
            capabilityReplied = true;
            continue;
        }

        if (event->event == IoCapabilityResponseEvent
            && event->length >= sizeof(IoCapabilityResponseEventParameters)) {
            const auto *response =
                reinterpret_cast<const IoCapabilityResponseEventParameters *>(payload);
            if (sameAddress(response->bdaddr, target)) {
                std::puts("NORTHSTAR_BLUETOOTH_HCI_IO_CAPABILITY_RESPONSE=1");
                std::fflush(stdout);
                if (!capabilityReplied) {
                    ng_hci_io_capability_request_reply_cp reply{};
                    reply.bdaddr = target;
                    reply.io_capability = IoCapabilityDisplayYesNo;
                    reply.oob_data_present = 0;
                    reply.authentication_requirements =
                        AuthenticationGeneralBondingMitm;
                    if (!sendCommand(socket,
                            NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
                                          NG_HCI_IO_CAPABILITY_REQUEST_REPLY),
                            &reply, sizeof(reply))) {
                        bt_devclose(socket);
                        return fail(ExitUnavailable,
                                    "the SSP compatibility capability reply failed");
                    }
                    capabilityReplied = true;
                    std::puts("NORTHSTAR_BLUETOOTH_HCI_IO_CAPABILITY_FALLBACK=1");
                    std::fflush(stdout);
                }
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_USER_CONFIRMATION_REQUEST
            && event->length >= sizeof(ng_hci_user_confirmation_request_ep)) {
            const auto *request = reinterpret_cast<const ng_hci_user_confirmation_request_ep *>(payload);
            if (!sameAddress(request->bdaddr, target))
                continue;
            std::printf("NORTHSTAR_BLUETOOTH_CONFIRM=%06u\n",
                        le32toh(request->numeric_value) % 1000000U);
            std::fflush(stdout);
            bool accepted = false;
            if (!readDecision(&accepted)) {
                bt_devclose(socket);
                return fail(ExitUnavailable, "Bluetooth confirmation timed out");
            }
            ng_hci_user_confirmation_request_reply_cp reply{};
            reply.bdaddr = target;
            const uint16_t opcode = accepted
                ? NG_HCI_USER_CONFIRMATION_REQUEST_REPLY
                : NG_HCI_USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY;
            if (!sendCommand(socket,
                    NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL, opcode),
                    &reply, sizeof(reply))) {
                bt_devclose(socket);
                return fail(ExitUnavailable, "the Bluetooth confirmation reply failed");
            }
            confirmationAnswered = accepted;
            if (!accepted) {
                bt_devclose(socket);
                return fail(125, "Bluetooth pairing was rejected");
            }
            continue;
        }

        if (event->event == NG_HCI_EVENT_SIMPLE_PAIRING_COMPLETE
            && event->length >= sizeof(ng_hci_simple_pairing_complete_ep)) {
            const auto *complete =
                reinterpret_cast<const ng_hci_simple_pairing_complete_ep *>(payload);
            if (!sameAddress(complete->bdaddr, target))
                continue;
            std::printf("NORTHSTAR_BLUETOOTH_HCI_PAIRING_STATUS=0x%02x\n",
                        complete->status);
            std::fflush(stdout);
            if (complete->status != 0) {
                bt_devclose(socket);
                return fail(ExitUnavailable, "Secure Simple Pairing did not complete");
            }
            pairingComplete = true;
            if (authenticationComplete) {
                std::puts(confirmationAnswered
                    ? "NORTHSTAR_BLUETOOTH_PAIRED=CONFIRMED"
                    : "NORTHSTAR_BLUETOOTH_PAIRED=JUST_WORKS");
                std::fflush(stdout);
                bt_devclose(socket);
                return 0;
            }
            continue;
        }
    }
}
