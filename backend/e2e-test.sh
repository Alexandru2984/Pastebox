#!/bin/bash
# PasteBox E2E Flow Tests
# Tests the complete user flows against the live service

set -euo pipefail

BASE="http://localhost:7777"
PASS=0 FAIL=0

green()  { echo -e "\033[32m  ✓ $1\033[0m"; }
red()    { echo -e "\033[31m  ✗ $1\033[0m"; }
yellow() { echo -e "\033[33m$1\033[0m"; }
blue()   { echo -e "\033[34m$1\033[0m"; }

assert() {
  local name="$1" expected="$2" actual="$3"
  if [ "$expected" = "$actual" ]; then green "$name"; PASS=$((PASS+1))
  else red "$name (expected: $expected, got: $actual)"; FAIL=$((FAIL+1)); fi
}

assert_contains() {
  local name="$1" needle="$2" haystack="$3"
  if echo "$haystack" | grep -q "$needle"; then green "$name"; PASS=$((PASS+1))
  else red "$name (expected to contain: $needle)"; FAIL=$((FAIL+1)); fi
}

echo "========================================="
blue "PasteBox E2E Flow Tests"
blue "Server: $BASE"
echo "========================================="

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 1: Create → View → Edit → Fork → Delete
# ═══════════════════════════════════════════════════════════════════════════
yellow "▸ Flow 1: Full paste lifecycle (Create → View → Edit → Fork → Delete)"

# Step 1: Create a paste
CREATE_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"E2E Test Paste","content":"function hello() { return 42; }","language":"javascript","visibility":"public","tags":["e2e","test"]}')
PASTE_ID=$(echo "$CREATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)
assert "Create paste returns ID" "true" "$([ -n "$PASTE_ID" ] && echo true || echo false)"

# Step 2: View the paste
VIEW_RESP=$(curl -s "$BASE/api/pastes/$PASTE_ID")
VIEW_TITLE=$(echo "$VIEW_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('title',''))" 2>/dev/null)
VIEW_CONTENT=$(echo "$VIEW_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('content',''))" 2>/dev/null)
assert "View returns correct title" "E2E Test Paste" "$VIEW_TITLE"
assert "View returns correct content" "function hello() { return 42; }" "$VIEW_CONTENT"

# Step 3: Verify it appears in list
LIST_RESP=$(curl -s "$BASE/api/pastes")
assert_contains "Paste appears in public list" "$PASTE_ID" "$LIST_RESP"

# Step 4: Edit the paste
EDIT_RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$PASTE_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"E2E Edited Paste","content":"function hello() { return 99; }","language":"typescript"}')
EDIT_CODE=$(echo "$EDIT_RESP" | tail -1)
assert "Edit returns 200" "200" "$EDIT_CODE"

# Step 5: Verify edit persisted
VIEW2_RESP=$(curl -s "$BASE/api/pastes/$PASTE_ID")
VIEW2_TITLE=$(echo "$VIEW2_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('title',''))" 2>/dev/null)
VIEW2_LANG=$(echo "$VIEW2_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('language',''))" 2>/dev/null)
assert "Edit title persisted" "E2E Edited Paste" "$VIEW2_TITLE"
assert "Edit language persisted" "typescript" "$VIEW2_LANG"

# Step 6: Fork the paste
FORK_RESP=$(curl -s -X POST "$BASE/api/pastes/$PASTE_ID/fork" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"E2E Forked Paste"}')
FORK_ID=$(echo "$FORK_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)
FORK_PARENT=$(echo "$FORK_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('parent_id',''))" 2>/dev/null)
assert "Fork returns new ID" "true" "$([ -n "$FORK_ID" ] && [ "$FORK_ID" != "$PASTE_ID" ] && echo true || echo false)"
assert "Fork has correct parent" "$PASTE_ID" "$FORK_PARENT"

# Step 7: Delete original
DEL_RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PASTE_ID" \
  -H 'X-Requested-With: PasteBox')
DEL_CODE=$(echo "$DEL_RESP" | tail -1)
assert "Delete original returns 200" "200" "$DEL_CODE"

# Step 8: Verify deleted
GONE_RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
GONE_CODE=$(echo "$GONE_RESP" | tail -1)
assert "Deleted paste returns 404" "404" "$GONE_CODE"

# Step 9: Fork still exists independently
FORK_CHECK=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$FORK_ID")
FORK_CODE=$(echo "$FORK_CHECK" | tail -1)
assert "Fork survives parent deletion" "200" "$FORK_CODE"

# Cleanup fork
curl -s -X DELETE "$BASE/api/pastes/$FORK_ID" -H 'X-Requested-With: PasteBox' >/dev/null

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 2: Password Protection Flow
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 2: Password protection lifecycle"

# Create password-protected paste
PW_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Secret Paste","content":"classified data","language":"plaintext","password":"e2epass","visibility":"unlisted"}')
PW_ID=$(echo "$PW_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)
assert "Protected paste created" "true" "$([ -n "$PW_ID" ] && echo true || echo false)"

# Try without password
NO_PW_RESP=$(curl -s "$BASE/api/pastes/$PW_ID")
PW_REQUIRED=$(echo "$NO_PW_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('password_required',False))" 2>/dev/null)
assert "Without password: password_required=True" "True" "$PW_REQUIRED"

# Try with wrong password
WRONG_RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID" -H 'X-Password: wrongpass')
WRONG_CODE=$(echo "$WRONG_RESP" | tail -1)
assert "Wrong password returns 404 (anti-enumeration)" "404" "$WRONG_CODE"

# Try with correct password
RIGHT_RESP=$(curl -s "$BASE/api/pastes/$PW_ID" -H 'X-Password: e2epass')
RIGHT_CONTENT=$(echo "$RIGHT_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('content',''))" 2>/dev/null)
assert "Correct password returns content" "classified data" "$RIGHT_CONTENT"

# Edit with password
EDIT_PW_RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$PW_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' -H 'X-Password: e2epass' \
  -d '{"content":"updated classified data"}')
EDIT_PW_CODE=$(echo "$EDIT_PW_RESP" | tail -1)
assert "Edit with password returns 200" "200" "$EDIT_PW_CODE"

# Edit without password fails
EDIT_NOPW_RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$PW_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"content":"hacked"}')
EDIT_NOPW_CODE=$(echo "$EDIT_NOPW_RESP" | tail -1)
assert "Edit without password returns 404" "404" "$EDIT_NOPW_CODE"

# Delete without password fails
DEL_NOPW=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PW_ID" -H 'X-Requested-With: PasteBox')
DEL_NOPW_CODE=$(echo "$DEL_NOPW" | tail -1)
assert "Delete without password returns 403" "403" "$DEL_NOPW_CODE"

# Delete with password works
DEL_PW=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PW_ID" \
  -H 'X-Requested-With: PasteBox' -H 'X-Password: e2epass')
DEL_PW_CODE=$(echo "$DEL_PW" | tail -1)
assert "Delete with password returns 200" "200" "$DEL_PW_CODE"

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 3: Burn After Read
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 3: Burn after read lifecycle"

BURN_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Self Destructing","content":"this will disappear","language":"plaintext","burn_after_read":true}')
BURN_ID=$(echo "$BURN_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)
assert "Burn paste created" "true" "$([ -n "$BURN_ID" ] && echo true || echo false)"

# First read should work
BURN_READ1=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
BURN_CODE1=$(echo "$BURN_READ1" | tail -1)
assert "First read of burn paste returns 200" "200" "$BURN_CODE1"

# Second read should fail
BURN_READ2=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
BURN_CODE2=$(echo "$BURN_READ2" | tail -1)
assert "Second read of burn paste returns 404" "404" "$BURN_CODE2"

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 4: Pagination Flow
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 4: Pagination flow"

# Create 5 pastes
for i in $(seq 1 5); do
  curl -s -X POST "$BASE/api/pastes" \
    -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
    -d "{\"title\":\"Page Test $i\",\"content\":\"page test content $i\",\"language\":\"plaintext\",\"visibility\":\"public\"}" >/dev/null
done

# Get page 1 with limit 2
PAGE1_RESP=$(curl -s "$BASE/api/pastes?limit=2&page=1")
PAGE1_COUNT=$(echo "$PAGE1_RESP" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d.get('data',[])))" 2>/dev/null)
PAGE1_HAS_MORE=$(echo "$PAGE1_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('has_more',False))" 2>/dev/null)
assert "Page 1 returns 2 items" "2" "$PAGE1_COUNT"
assert "Page 1 has_more is True" "True" "$PAGE1_HAS_MORE"

# Get page 2
PAGE2_RESP=$(curl -s "$BASE/api/pastes?limit=2&page=2")
PAGE2_COUNT=$(echo "$PAGE2_RESP" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d.get('data',[])))" 2>/dev/null)
assert "Page 2 returns 2 items" "2" "$PAGE2_COUNT"

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 5: Tag Filtering
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 5: Tag filtering flow"

TAG_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Tagged Paste","content":"has special tags","language":"python","visibility":"public","tags":["unique-e2e-tag"]}')
TAG_ID=$(echo "$TAG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)

# Filter by unique tag
FILTER_RESP=$(curl -s "$BASE/api/pastes?tag=unique-e2e-tag")
FILTER_COUNT=$(echo "$FILTER_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('data',[])))" 2>/dev/null)
assert "Tag filter returns matching pastes" "true" "$([ "$FILTER_COUNT" -ge 1 ] && echo true || echo false)"
assert_contains "Tag filter result contains our paste" "$TAG_ID" "$FILTER_RESP"

# Filter by nonexistent tag
EMPTY_RESP=$(curl -s "$BASE/api/pastes?tag=nonexistent-tag-xyz")
EMPTY_COUNT=$(echo "$EMPTY_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('data',[])))" 2>/dev/null)
assert "Nonexistent tag returns 0 results" "0" "$EMPTY_COUNT"

# Cleanup
curl -s -X DELETE "$BASE/api/pastes/$TAG_ID" -H 'X-Requested-With: PasteBox' >/dev/null

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 6: Visibility Controls
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 6: Visibility controls"

# Create unlisted paste
UNLISTED_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Unlisted E2E","content":"unlisted content","language":"plaintext","visibility":"unlisted"}')
UNLISTED_ID=$(echo "$UNLISTED_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)

# Should not appear in public list
LIST_CHECK=$(curl -s "$BASE/api/pastes?limit=100")
assert "Unlisted paste not in public list" "false" "$(echo "$LIST_CHECK" | grep -q "$UNLISTED_ID" && echo true || echo false)"

# But should be accessible directly
DIRECT_RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$UNLISTED_ID")
DIRECT_CODE=$(echo "$DIRECT_RESP" | tail -1)
assert "Unlisted paste accessible via direct link" "200" "$DIRECT_CODE"

# Create private paste
PRIVATE_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Private E2E","content":"private content","language":"plaintext","visibility":"private"}')
PRIVATE_ID=$(echo "$PRIVATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)

# Should not appear in public list
LIST_CHECK2=$(curl -s "$BASE/api/pastes?limit=100")
assert "Private paste not in public list" "false" "$(echo "$LIST_CHECK2" | grep -q "$PRIVATE_ID" && echo true || echo false)"

# Cleanup
curl -s -X DELETE "$BASE/api/pastes/$UNLISTED_ID" -H 'X-Requested-With: PasteBox' >/dev/null
curl -s -X DELETE "$BASE/api/pastes/$PRIVATE_ID" -H 'X-Requested-With: PasteBox' >/dev/null

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 7: Raw Endpoint
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 7: Raw content endpoint"

RAW_PASTE=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Raw Test","content":"print(\"hello world\")","language":"python","visibility":"public"}')
RAW_ID=$(echo "$RAW_PASTE" | python3 -c "import sys,json; print(json.load(sys.stdin).get('id',''))" 2>/dev/null)

RAW_RESP=$(curl -s "$BASE/api/pastes/$RAW_ID/raw")
assert "Raw endpoint returns plain content" 'print("hello world")' "$RAW_RESP"

RAW_HEADERS=$(curl -sI "$BASE/api/pastes/$RAW_ID/raw")
assert_contains "Raw has Content-Type text/plain" "text/plain" "$RAW_HEADERS"
assert_contains "Raw has Content-Disposition inline" "inline" "$RAW_HEADERS"

# Cleanup
curl -s -X DELETE "$BASE/api/pastes/$RAW_ID" -H 'X-Requested-With: PasteBox' >/dev/null

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 8: Frontend Serves Correctly
# ═══════════════════════════════════════════════════════════════════════════
sleep 0.3
yellow "▸ Flow 8: Frontend serving"

FE_RESP=$(curl -s -w "\n%{http_code}" "$BASE/")
FE_CODE=$(echo "$FE_RESP" | tail -1)
assert "Frontend serves index.html (200)" "200" "$FE_CODE"
assert_contains "HTML has PasteBox title" "PasteBox" "$(echo "$FE_RESP" | head -n -1)"
assert_contains "HTML has Svelte app root" "app" "$(echo "$FE_RESP" | head -n -1)"

# Static assets
ASSETS_RESP=$(curl -s -o /dev/null -w "%{http_code}" "$BASE/favicon.svg")
assert "Favicon serves (200)" "200" "$ASSETS_RESP"

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 9: Health Check
# ═══════════════════════════════════════════════════════════════════════════
yellow "▸ Flow 9: Health check"

HEALTH_RESP=$(curl -s "$BASE/api/health")
HEALTH_STATUS=$(echo "$HEALTH_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null)
HEALTH_DB=$(echo "$HEALTH_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('db',''))" 2>/dev/null)
assert "Health status is ok" "ok" "$HEALTH_STATUS"
assert "Health DB connected" "connected" "$HEALTH_DB"

# ═══════════════════════════════════════════════════════════════════════════
# FLOW 10: CSRF Protection
# ═══════════════════════════════════════════════════════════════════════════
yellow "▸ Flow 10: CSRF enforcement"

CSRF_RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"CSRF test","content":"should fail","language":"plaintext"}')
CSRF_CODE=$(echo "$CSRF_RESP" | tail -1)
assert "POST without CSRF header rejected (403)" "403" "$CSRF_CODE"

CSRF_PUT=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/any" \
  -H 'Content-Type: application/json' \
  -d '{"title":"test"}')
CSRF_PUT_CODE=$(echo "$CSRF_PUT" | tail -1)
assert "PUT without CSRF header rejected (403)" "403" "$CSRF_PUT_CODE"

CSRF_DEL=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/any")
CSRF_DEL_CODE=$(echo "$CSRF_DEL" | tail -1)
assert "DELETE without CSRF header rejected (403)" "403" "$CSRF_DEL_CODE"

# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo "========================================="
if [ $FAIL -eq 0 ]; then
  green "ALL $PASS E2E TESTS PASSED ✓"
else
  red "$FAIL/$((PASS+FAIL)) E2E TESTS FAILED"
  green "$PASS/$((PASS+FAIL)) tests passed"
fi
echo "========================================="
exit $FAIL
