# Feature Landscape

**Domain:** LAN-only keyboard-mouse sharing (software KVM)
**Researched:** 2026-05-12
**Context:** Deskflow personal fork, macOS + Windows only, stripped down for speed

---

## Table Stakes

Features every user expects from an input sharing tool. Missing any of these means the product feels broken or incomplete.

| Feature | Why Expected | Complexity | Status in Deskflow | Notes |
|---------|--------------|------------|--------------------|-------|
| Mouse cursor movement across screen edges | This IS the product. Without seamless edge-crossing there is no reason to use the tool. | Low | Implemented | Current protocol uses absolute coordinates (DMMV). Works but latency-sensitive. |
| Keyboard input forwarding | Users need to type on the remote machine. Second most fundamental operation. | Medium | Implemented | Protocol handles key down/up/repeat with modifier masks. KeyButton tracking for dead keys is solid. |
| Mouse button and scroll forwarding | Clicking and scrolling must work. Users notice immediately if buttons or wheel are wrong. | Low | Implemented | Protocol supports left/right/middle + extra buttons. Wheel has horizontal + vertical. |
| Clipboard text sharing | Users copy text on one machine and expect to paste on the other. Every competitor has this. Not having it is a dealbreaker. | Medium | Implemented | Chunked transfer for large clipboard content (v1.6+ protocol). Works for text and images. |
| Screen edge configuration | Users need to tell the software where each screen is relative to others. | Low | Implemented | Grid-based layout in GUI. |
| Scroll Lock toggle to lock cursor | Prevents accidental screen switching. Barrier popularized this. Games and full-screen apps make this essential. | Low | Implemented | Scroll Lock key toggle is standard across Synergy/Barrier lineage. |
| Auto-reconnect after network interruption | LAN setups have transient disconnects. Users should not need to manually restart. | Medium | Implemented | Keep-alive mechanism (CALV every 3s, 3 missed = dead). But current reconnect behavior may be janky. |
| Modifier key sync (Caps Lock, Num Lock) | If Caps Lock state is wrong on the remote machine, users get wrong input. Extremely frustrating. | Medium | Implemented | kMsgCEnter carries modifier mask. Synced on screen entry. |
| Smooth cursor transition | The cursor must not jump, stutter, or lag noticeably when crossing screens. | High | Partially working | This is the CORE VALUE per PROJECT.md. Currently broken under network jitter due to TCP + blocking I/O in SocketMultiplexer. |

---

## Nice-to-Have Features

Features that improve the experience significantly. Users do not expect these, but they add real value.

| Feature | Value Proposition | Complexity | Status in Deskflow | Notes |
|---------|-------------------|------------|--------------------|-------|
| Clipboard image sharing | Copy a screenshot on Mac, paste on PC. Common workflow for designers and developers. | Medium | Implemented | Protocol supports it. Image data sent as clipboard chunks. |
| File drag-and-drop transfer | Drag a file from one desktop to another. Very convenient when it works. | High | Protocol exists, feature deprecated | kMsgDDragInfo and kMsgDFileTransfer messages exist in protocol but marked as deprecated / no longer implemented. ShareMouse does this well (~60 MiB/s over gigabit). Would require reimplementation. |
| Screen saver synchronization | Start/stop screensaver across all machines simultaneously. | Low | Implemented | kMsgCScreenSaver (CSEC) message. Works but has known bugs on Windows/Mac per legacy FAQ. Low value for a personal tool. |
| Keyboard layout synchronization | Automatically match keyboard language/layout across machines. | Medium | Implemented (v1.8) | kMsgDKeyDownLang and kMsgDLanguageSynchronisation. Useful for bilingual users. |
| Relative mouse movement | Better for gaming and high-precision input. | Low | Implemented | kMsgDMouseRelMove (DMRM). Important for games on the client machine. |
| Secure input notification (macOS) | Shows which app has requested secure input mode. Prevents confusion when keyboard stops working because a password field grabbed it. | Low | Implemented (v1.7) | kMsgDSecureInputNotification. macOS-specific. |
| Multi-monitor layout profiles | Save and switch between different monitor arrangements. | Low | Not implemented | ShareMouse has this. Useful if you reconfigure your desk often. Low priority for personal use. |
| Pause/unpause with hotkey (not just Scroll Lock) | Quickly disable input sharing without toggling Scroll Lock. | Low | Partially implemented | Barrier uses Scroll Lock toggle. ShareMouse uses a hold-down key. A dedicated pause hotkey would be nice. |
| Latency metrics display | Show current round-trip time so users can diagnose network issues. | Medium | Not implemented | LAN Mouse has this on their roadmap. Would help users understand if lag is network or software. |
| Portable client mode | Run the client from a USB stick without installing. | Low | Not implemented | ShareMouse offers this. Useful for temporary setups. |
| Lock all screens simultaneously | Lock every connected machine at once when you walk away. | Low | Not implemented | Mouse without Borders has this. Trivial to implement via protocol. |

---

## Anti-Features

Features to explicitly NOT build. These add complexity, maintenance burden, or attack surface without proportionate value for a personal LAN-only tool.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| TLS/SSL encryption | LAN-only use case. Encryption adds ~3000+ lines of code, certificate management complexity, and latency overhead. The current TLS layer in Deskflow is known to have certificate validation bypass bugs. PROJECT.md explicitly lists this for removal. | Remove entirely. If security is ever needed, add simple password-based auth at the protocol level. |
| Linux platform support (X11/Wayland/EI) | User only uses macOS + Windows. Linux support adds ~5000 lines of platform code, Wayland input capture complexity, and libei dependencies. The X11/Wayland situation is a moving target (Wayland has no stable input injection API). | Remove all Linux/X11/Wayland/EI code paths. Focus platform layer on macOS (CGEvent) and Windows (SendInput). |
| Windows daemon service mode (deskflow-daemon) | Running as a Windows service adds complexity for privilege escalation, session isolation, and UAC interaction. A user-mode GUI app is simpler and sufficient for personal use. | Remove. Run as a normal user-mode application with accessibility permissions. |
| Multi-language GUI translation | Maintaining translations across every UI change is a chore. For a personal tool, English-only is fine. | Freeze translations. Accept English-only UI. |
| Commercial licensing / activation system | Not applicable to a personal open-source fork. Adds complexity with license servers, activation flows, and DRM. | Remove any license checking code. Keep MIT license. |
| File drag-and-drop (for now) | The protocol messages exist but the feature is deprecated. Reimplementing it correctly across platforms is high-complexity (mime type handling, async file I/O, progress reporting, cancellation). ShareMouse's implementation is a key differentiator but requires significant investment. | Defer. Focus on core latency first. Clipboard text+image sharing covers most transfer needs. Revisit after core experience is rock-solid. |
| Screen saver sync (keep but de-prioritize) | Known bugs on Windows/Mac. Low value for personal use. Adds testing surface. | Keep the protocol message but do not invest in fixing edge cases. If it breaks, disable rather than fix. |
| Universal Control-style auto-discovery | Apple uses Bluetooth + WiFi for seamless discovery. Implementing Bonjour/mDNS adds networking complexity and dependencies. For a 2-machine setup, manual IP is fine. | Use manual IP configuration. Consider adding Bonjour later as a stretch goal if setup friction becomes a real complaint. |
| Clipboard format negotiation for rich content | Supporting HTML, RTF, and proprietary formats across platforms is a rabbit hole. Each platform has different clipboard formats and conversion is error-prone. | Support plain text and images only. Strip formatting on cross-platform transfer. |

---

## Feature Dependencies

```
Core input forwarding (mouse + keyboard)
  |
  +---> Modifier key sync (requires keyboard forwarding)
  |
  +---> Smooth cursor transition (requires mouse forwarding + low-latency network layer)
  |       |
  |       +---> Relative mouse movement (requires mouse forwarding, benefits from low latency)
  |
  +---> Scroll Lock toggle (requires keyboard forwarding)
  |
  +---> Screen edge configuration (requires mouse forwarding)
  |
  +---> Auto-reconnect (requires connection management, independent of input)
  |
  +---> Clipboard text sharing (semi-independent, uses same network connection)
  |       |
  |       +---> Clipboard image sharing (extends clipboard, same mechanism)
  |
  +---> Keyboard layout sync (requires keyboard forwarding)
  |
  +---> Secure input notification (requires keyboard forwarding, macOS-only)
  |
  +---> Screen saver sync (mostly independent, uses keep-alive channel)
```

**Critical dependency chain:** The entire product value depends on low-latency input forwarding. Everything else is secondary.

---

## Latency Analysis: How Competitors Handle Input

### Barrier / Deskflow (Current Approach)
- **Protocol:** TCP with blocking I/O via SocketMultiplexer
- **Latency characteristics:** Uses `poll()` with infinite timeout (`-1`). Single-threaded service loop processes all sockets sequentially. Under network jitter, TCP retransmission causes head-of-line blocking -- one delayed packet stalls ALL subsequent input events.
- **Keep-alive:** 3-second interval, 3 missed = disconnect. This is coarse.
- **Reported latency:** ~45ms end-to-end
- **Root cause of stuttering:** TCP + blocking I/O means a single lost packet or delayed ACK freezes the entire input pipeline. The SocketMultiplexer uses manual `new`/`delete` with complex cursor-based linked list management, adding overhead to each event cycle.

### ShareMouse
- **Protocol:** Proprietary, likely UDP-based with custom reliability layer
- **Reported latency:** <3.4ms
- **How it achieves this:** ShareMouse does not publish internals, but the ~10x latency advantage over TCP-based tools strongly suggests UDP transport with no head-of-line blocking. Input events are sent as individual datagrams -- if one is lost, the next one (with a newer cursor position) supersedes it.
- **Key insight:** For mouse movement, packet loss is acceptable because the next packet has a more recent position. Only keyboard events need guaranteed delivery.

### LAN Mouse (Rust-based)
- **Protocol:** UDP with DTLS encryption (WebRTC.rs)
- **Latency:** Not formally benchmarked, but the event-driven Rust architecture avoids the blocking-I/O trap
- **Architecture insight:** Separates input capture from input emulation. Uses platform-native event APIs directly. No shared multiplexer thread bottleneck.
- **Encryption overhead:** DTLS adds some latency but is optional.

### Mouse Without Borders (PowerToys)
- **Protocol:** TCP-based
- **Latency:** Similar to Barrier (~30-50ms range)
- **Notable:** Windows-only. Clipboard sync is generally fast for text but sluggish for large content.

### Apple Universal Control
- **Protocol:** Proprietary, uses both Bluetooth and WiFi Direct
- **Latency:** Near-native. Feels like a native multi-monitor setup.
- **Key insight:** System-level integration gives Apple access to input APIs that third-party tools cannot use. This is the gold standard but not replicable without OS vendor access.

### Recommendation for Deskflow

The single highest-impact change is **moving from TCP to UDP for input event transport**. Mouse movement packets should be fire-and-forget (each packet contains absolute coordinates; losing one is fine). Keyboard events can use a lightweight reliability layer on top of UDP (sequence numbers + ACK for key down/up only).

This alone would eliminate head-of-line blocking and bring latency from ~45ms to near-ShareMouse levels.

---

## MVP Recommendation

**Prioritize (Phase 1 -- Must Ship):**
1. Low-latency mouse/keyboard forwarding (UDP transport or at minimum, non-blocking TCP)
2. Clipboard text sharing (keep existing, ensure it does not block input events)
3. Screen edge configuration (keep existing)
4. Scroll Lock toggle (keep existing)
5. Auto-reconnect (keep existing, improve robustness)

**Defer (Phase 2 -- After Core is Solid):**
1. Clipboard image sharing (already works, just needs testing with new network layer)
2. Keyboard layout sync (already implemented, low risk)
3. File drag-and-drop (high complexity, defer until core is proven)

**Explicitly Remove:**
1. TLS/SSL encryption layer
2. Linux platform support (X11/Wayland/EI)
3. Windows daemon service mode
4. Commercial licensing code

---

## Confidence Assessment

| Area | Confidence | Reason |
|------|------------|--------|
| Table stakes features | HIGH | Directly verified in Deskflow protocol types and Barrier/ShareMouse documentation |
| Nice-to-have categorization | HIGH | Based on competitor feature analysis (ShareMouse, MWB, LAN Mouse) and user reviews |
| Anti-feature recommendations | HIGH | Aligned with PROJECT.md explicit removal targets; LATENCY is the core value, encryption is unnecessary |
| Latency analysis | MEDIUM | ShareMouse <3.4ms claim from one source (Alibaba LifeTips). Barrier ~45ms from same source. TCP head-of-line blocking analysis is from direct code reading of SocketMultiplexer.cpp. UDP recommendation is technically sound but unverified with a prototype. |
| File drag-and-drop deferral | MEDIUM | The protocol messages exist but the feature is deprecated. The complexity assessment is based on cross-platform mime/async/cancellation requirements. This could be revisited if clipboard-only workflow proves insufficient. |

---

## Sources

- Deskflow ProtocolTypes.h -- direct code analysis of all protocol messages
- Deskflow SocketMultiplexer.cpp -- direct code analysis of blocking I/O architecture
- [Barrier GitHub](https://github.com/debauchee/barrier) -- feature comparison, TCP protocol confirmed
- [LAN Mouse GitHub](https://github.com/feschber/lan-mouse) -- UDP/DTLS architecture, Rust-based
- [ShareMouse vs Synergy comparison (DukLabs)](https://blog.duklabs.com/synergy-vs-sharemouse/) -- hands-on latency comparison, feature breakdown
- [ShareMouse official site](https://www.sharemouse.com/) -- feature list, <3.4ms latency claim
- [Alibaba LifeTips -- ShareMouse vs Barrier latency](https://lifetips.alibaba.com/tech-efficiency/sharemouse-controls-multiple-computers-with-a-single-keyboard-mouse/) -- quantitative latency comparison (ShareMouse <3.4ms, Barrier <45ms)
- [Barrier UDP issue #863](https://github.com/debauchee/barrier/issues/863) -- confirms TCP-only, UDP planned but not implemented
- [Deskflow Discussion #8082](https://github.com/deskflow/deskflow/discussions/8082) -- confirms no file drag-and-drop in current Deskflow, clipboard text+images only
- [Synergy ChangeLog](https://github.com/jherig/synergy/blob/master/ChangeLog) -- screen saver sync UI removed, file drag-drop UI removed
- [Reddit -- Synergy/Deskflow/InputLeap/Barrier differences](https://www.reddit.com/r/linux/comments/1g6atbf/synergy_deskflow_input_leap_barrier_whats_the/) -- ecosystem lineage
