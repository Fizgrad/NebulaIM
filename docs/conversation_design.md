# Conversation Design

`conversation_id` is still deterministic, but a production IM needs a per-user conversation view. NebulaIM adds `conversations`, where each user owns their own row for the same logical conversation.

Single chat writes two rows: sender and receiver. Group chat can write one row per member. This enables efficient list queries by `owner_user_id`, `updated_at`, pinned state, and unread count.

Stored fields include conversation type, peer user, group id, last message id, preview, last message time, unread count, pinned, muted, deleted, created time, and updated time.

Unread logic: the sender's conversation is updated without unread increment; receivers get unread increment. `MarkConversationRead` clears unread count and read receipt APIs can update message-level read state.

Deletion is soft delete per user. Pin and mute are per-user preferences. For large groups, per-member conversation fanout can later move to async workers or lazy materialization.
