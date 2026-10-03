/*
 * KVM input redirection hooks -- added by the moonlight-kvm-input fork.
 *
 * WHY
 *   The fork routes input to a GL.iNet KVM's HID API instead of the GameStream
 *   protocol. Every LiSend* entry point in InputStream.c funnels through this
 *   hook table, so an intercepted event CANNOT also be sent to the host over
 *   GameStream: each hooked function returns immediately. Redirecting at the
 *   49 call sites in moonlight-qt instead would leave any site you missed
 *   silently reintroducing synthetic input.
 *
 * CONTRACT
 *   - All pointers are NULL by default. NULL means "do not intercept" and the
 *     upstream GameStream behaviour is preserved exactly. A client that never
 *     calls LiSetKvmInputHooks() gets a byte-identical upstream library.
 *   - The hooks are installed once, before any input is generated. They are not
 *     synchronised; do not change them while a session is running.
 *   - A hook returning non-zero is returned to the caller as the LiSend* result.
 *     0 means "handled".
 *   - If the transport behind a hook has died, the hook must still return without
 *     sending anything over GameStream. Falling back would put synthetic input on
 *     the controlled machine mid-session, which is the exact hazard this design
 *     exists to remove. Drop the event and surface the error to the user instead.
 *
 * COPYRIGHT
 *   This file is part of a fork and is licensed under the same terms as
 *   moonlight-common-c (GPLv3). See the upstream LICENSE.
 */

#ifndef KVM_INPUT_HOOKS_H
#define KVM_INPUT_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _KVM_INPUT_HOOKS {
    /*
     * Implemented paths. Each has a direct equivalent in the KVM's JSON
     * WebSocket API (mouse_relative, mouse_move, mouse_button, key,
     * mouse_wheel).
     */

    /* KVM mouse_relative. Deltas are signed and must be split to +/-127. */
    int (*sendMouseMove)(short deltaX, short deltaY);

    /*
     * KVM mouse_move. Only reachable in absolute mouse mode, which is NOT used
     * here (the device reports mouse.absolute: false, output usb_rel), but it
     * must be hooked all the same so it cannot leak to GameStream.
     */
    int (*sendMousePosition)(short x, short y, short referenceWidth, short referenceHeight);

    /* KVM mouse_button. */
    int (*sendMouseButton)(char action, int button);

    /*
     * KVM key. keyCode is a Windows VK code with 0x8000 set; keyAction is
     * KEY_ACTION_DOWN/UP; modifiers is a MODIFIER_* bitmask which the KVM has no
     * concept of, so the implementation must synthesise discrete modifier key
     * events (and release them on teardown).
     */
    int (*sendKeyboard)(short keyCode, char keyAction, char modifiers, char flags);

    /* KVM mouse_wheel, vertical. Amount is in LI_WHEEL_DELTA units. */
    int (*sendHighResScroll)(short scrollAmount);

    /* KVM mouse_wheel, horizontal. */
    int (*sendHighResHScroll)(short scrollAmount);

    /*
     * Catch-all for everything with no KVM equivalent: UTF-8 text
     * (CTRL_CHANNEL_UTF8 -- a separate channel, and the KVM JSON API has no
     * "type this string" event), controllers, touch and pen.
     *
     * When non-NULL these events are DROPPED rather than sent, so they cannot
     * reach the host as synthetic input. "kind" is a static string naming the
     * source ("text", "controller", "touch", "pen") for logging.
     */
    int (*dropInput)(const char *kind);
} KVM_INPUT_HOOKS;

/*
 * Install the hook table. Pass NULL to restore upstream behaviour.
 *
 * Call before LiStartConnection() and clear after LiStopConnection(), having
 * first released anything still held on the KVM side.
 */
void LiSetKvmInputHooks(const KVM_INPUT_HOOKS *hooks);

#ifdef __cplusplus
}
#endif

#endif /* KVM_INPUT_HOOKS_H */
