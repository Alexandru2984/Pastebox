#!/bin/bash
# PasteBox Backend Integration Tests
# Run against a live server (default http://localhost:7777)

BASE="${1:-http://localhost:7777}"
PASS=0
FAIL=0
TOTAL=0

red() { echo -e "\e[31m$1\e[0m"; }
green() { echo -e "\e[32m$1\e[0m"; }
yellow() { echo -e "\e[33m$1\e[0m"; }

assert() {
  TOTAL=$((TOTAL + 1))
  local desc="$1" expected="$2" actual="$3"
  if [ "$expected" = "$actual" ]; then
    green "  ✓ $desc"
    PASS=$((PASS + 1))
  else
    red "  ✗ $desc (expected: $expected, got: $actual)"
    FAIL=$((FAIL + 1))
  fi
}

assert_contains() {
  TOTAL=$((TOTAL + 1))
  local desc="$1" needle="$2" haystack="$3"
  if echo "$haystack" | grep -q "$needle"; then
    green "  ✓ $desc"
    PASS=$((PASS + 1))
  else
    red "  ✗ $desc (expected to contain: $needle)"
    FAIL=$((FAIL + 1))
  fi
}

assert_not_contains() {
  TOTAL=$((TOTAL + 1))
  local desc="$1" needle="$2" haystack="$3"
  if echo "$haystack" | grep -qv "$needle" || ! echo "$haystack" | grep -q "$needle"; then
    green "  ✓ $desc"
    PASS=$((PASS + 1))
  else
    red "  ✗ $desc (should NOT contain: $needle)"
    FAIL=$((FAIL + 1))
  fi
}

echo "========================================="
echo "PasteBox Backend Integration Tests"
echo "Server: $BASE"
echo "========================================="
echo ""

# ─── Test 1: Create a basic paste ───
yellow "▸ Create Paste"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Test Paste","content":"print(42)","language":"python","visibility":"public"}')
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "POST /api/pastes returns 201" "201" "$CODE"
PASTE_ID=$(echo "$BODY" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
assert "Response contains id" "true" "$([ -n "$PASTE_ID" ] && echo true || echo false)"
assert_contains "Response contains title" '"title":"Test Paste"' "$BODY"

# ─── Test 2: Get paste ───
yellow "▸ Get Paste"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes/:id returns 200" "200" "$CODE"
assert_contains "Contains content" '"content":"print(42)"' "$BODY"
assert_contains "Has view count" '"views"' "$BODY"

# ─── Test 3: List pastes ───
yellow "▸ List Pastes"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes returns 200" "200" "$CODE"
assert_contains "List contains our paste" "$PASTE_ID" "$BODY"

# ─── Test 4: Create paste with tags and filter ───
yellow "▸ Tags"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Tagged","content":"test","language":"plaintext","visibility":"public","tags":["alpha","beta"]}')
TAG_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes?tag=alpha")
assert_contains "Tag filter returns tagged paste" "$TAG_ID" "$RESP"
RESP=$(curl -s "$BASE/api/pastes?tag=nonexistent999")
assert_not_contains "Tag filter excludes unmatched" "$TAG_ID" "$RESP"

# ─── Test 5: Password protection ───
yellow "▸ Password Protection"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Secret","content":"hidden data","language":"plaintext","visibility":"public","password":"mypass123"}')
PW_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Protected paste returns 403 without password" "403" "$CODE"
assert_contains "Response says password required" '"password_required":true' "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID" -H 'X-Password: wrongpass')
CODE=$(echo "$RESP" | tail -1)
assert "Wrong password returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID" -H 'X-Password: mypass123')
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "Correct password returns 200" "200" "$CODE"
assert_contains "Returns content with correct password" '"content":"hidden data"' "$BODY"

# ─── Test 6: Burn after read ───
yellow "▸ Burn After Read"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Burn Me","content":"ephemeral","language":"plaintext","visibility":"public","burn_after_read":true}')
BURN_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
CODE=$(echo "$RESP" | tail -1)
assert "First read of burn paste returns 200" "200" "$CODE"
assert_contains "First read has burned flag" '"burned":true' "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Second read of burned paste returns 404" "404" "$CODE"

# ─── Test 7: Visibility ───
yellow "▸ Visibility"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Unlisted","content":"hidden","language":"plaintext","visibility":"unlisted"}')
UNL_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes")
assert_not_contains "Unlisted paste not in public list" "$UNL_ID" "$RESP"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$UNL_ID")
assert "Unlisted paste accessible via direct link" "200" "$(echo "$RESP" | tail -1)"

# ─── Test 8: Raw endpoint ───
yellow "▸ Raw Endpoint"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID/raw")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes/:id/raw returns 200" "200" "$CODE"
assert_contains "Raw returns plain content" "print(42)" "$BODY"

# ─── Test 9: Fork ───
yellow "▸ Fork"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes/$PASTE_ID/fork" \
  -H 'Content-Type: application/json' -d '{}')
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "POST /api/pastes/:id/fork returns 201" "201" "$CODE"
FORK_ID=$(echo "$BODY" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$FORK_ID")
assert_contains "Forked paste has parent_id" "\"parent_id\":\"$PASTE_ID\"" "$RESP"

# ─── Test 10: Delete ───
yellow "▸ Delete"
RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PASTE_ID")
CODE=$(echo "$RESP" | tail -1)
assert "DELETE returns 200" "200" "$CODE"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Deleted paste returns 404" "404" "$CODE"

# ─── Test 11: Expiration ───
yellow "▸ Expiration"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Expiring","content":"bye","language":"plaintext","visibility":"public","expires_in":"1h"}')
EXP_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$EXP_ID")
assert_contains "Expiring paste has expires_at" '"expires_at"' "$RESP"

# ─── Test 12: Max size enforcement ───
yellow "▸ Max Size"
python3 -c "
import json, sys
data = {'title':'Big','content':'x'*600000,'language':'plaintext','visibility':'public'}
sys.stdout.write(json.dumps(data))
" > /tmp/pastebox_bigtest.json
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d @/tmp/pastebox_bigtest.json)
CODE=$(echo "$RESP" | tail -1)
assert "Oversized paste rejected (400 or 413)" "true" "$([ "$CODE" = "400" ] || [ "$CODE" = "413" ] && echo true || echo false)"
rm -f /tmp/pastebox_bigtest.json

# ─── Test 13: Empty content rejected ───
yellow "▸ Validation"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Empty","content":"","language":"plaintext","visibility":"public"}')
CODE=$(echo "$RESP" | tail -1)
assert "Empty content rejected (400)" "400" "$CODE"

# ─── Test 14: 404 for nonexistent paste ───
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/DOESNOTEXIST999")
CODE=$(echo "$RESP" | tail -1)
assert "Nonexistent paste returns 404" "404" "$CODE"

# ─── Cleanup test pastes ───
for id in $TAG_ID $PW_ID $UNL_ID $FORK_ID $EXP_ID; do
  curl -s -X DELETE "$BASE/api/pastes/$id" > /dev/null 2>&1
done

# ─── Summary ───
echo ""
echo "========================================="
if [ $FAIL -eq 0 ]; then
  green "ALL $TOTAL TESTS PASSED ✓"
else
  red "$FAIL/$TOTAL TESTS FAILED"
  green "$PASS/$TOTAL tests passed"
fi
echo "========================================="
exit $FAIL
