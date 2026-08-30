<div align="center">

<img src="../assets/logo_SUB_OS.png" alt="SUB-OS logo" width="140" height="140" />

# Security Policy

</div>

SUB-OS is an from-scratch, research/hobby operating system. It is built to be
correct and educational, **not** to be deployed as a hardened production system.
This document explains what is supported, how to report a vulnerability, and the
security caveats you should know before running it anywhere that matters.

---

## Supported versions

| Version | Codename | Status | Security fixes |
|---------|----------|--------|----------------|
| `1.0.0` (`1.0.x`) | Titan | ✅ Current stable | Yes |
| `0.2.x` / `0.0.x` (alpha/beta) | — | ⚠️ Superseded | No |

Only the current stable line receives security fixes. Please upgrade to the
latest [release](https://github.com/subhobhai943/SUB-OS/releases/latest) before
reporting an issue.

---

## Reporting a vulnerability

**Please do not open a public GitHub issue for a security vulnerability.**

Instead, report it privately through GitHub's coordinated-disclosure channel:

1. Go to the repository's **Security** tab →
   **[Report a vulnerability](https://github.com/subhobhai943/SUB-OS/security/advisories/new)**.
2. This opens a **private security advisory** visible only to you and the
   maintainers.
3. Include:
   - Affected version / commit SHA and architecture.
   - A description of the class of problem and its impact.
   - Minimal steps or a proof-of-concept that reproduces it.
   - Any suggested remediation, if you have one.

> Please describe the **class** of problem and its impact rather than publishing
> a weaponized, step-by-step exploit.

### What to expect

- **Acknowledgement:** we aim to confirm receipt within **7 days**.
- **Assessment:** an initial severity assessment and triage within **14 days**.
- **Fix & disclosure:** we coordinate a fix and a disclosure timeline with you,
  and credit you in the advisory and release notes (unless you prefer to remain
  anonymous).

---

## Known security caveats

SUB-OS deliberately trades several hardening properties for simplicity and
readability at this stage. These are **known and documented**, not bugs:

- **TLS has no certificate verification.** The TLS 1.3 client performs the key
  exchange and AEAD correctly but **does not validate the server certificate
  chain**. Connections are encrypted but **not authenticated** — they are
  trivially MITM-able. Do not use it to protect real secrets.
- **Default credentials are `SUB` / `SUB`.** Every image ships with the same
  well-known login. Treat any running instance as unauthenticated.
- **No exploit mitigations yet.** There is no ASLR, no stack canaries in kernel
  space, and W^X is not enforced on every mapping. Some segments load RWX.
- **Single-user, cooperative trust model.** Isolation between the ring-3 shell
  and the kernel is minimal; the userland is trusted.
- **Networking is best-effort.** The stack is a from-scratch implementation and
  has not been fuzzed or audited against hostile packets.

Run SUB-OS in a **VM or emulator (QEMU)**, on isolated networks, and never with
real credentials or sensitive data.

---

## Scope

In scope for a security report:

- Memory-safety faults reachable from the network stack, filesystem parsers, or
  the image/crypto codecs.
- Authentication or privilege-boundary bypasses beyond the documented caveats.
- Cryptographic implementation errors (e.g. a broken AEAD or key schedule).

Out of scope (already documented above): the default credentials, the absence
of TLS certificate verification, and the lack of exploit mitigations.

---

Thank you for helping keep SUB-OS and its users safe. 🐉
