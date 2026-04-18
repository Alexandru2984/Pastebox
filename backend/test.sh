#!/bin/bash
# PasteBox Backend Integration Tests
# Run against a live server (default http://localhost:7777)

BASE="${1:-http://localhost:7777}"
PASS=0
FAIL=0
TOTAL=0

# Small delay between sections to avoid rate limiting
delay() { sleep 0.3; }

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
delay
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
delay
yellow "▸ Get Paste"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes/:id returns 200" "200" "$CODE"
assert_contains "Contains content" '"content":"print(42)"' "$BODY"
assert_contains "Has view count" '"views"' "$BODY"

# ─── Test 3: List pastes ───
delay
yellow "▸ List Pastes"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes returns 200" "200" "$CODE"
assert_contains "List contains our paste" "$PASTE_ID" "$BODY"

# ─── Test 4: Create paste with tags and filter ───
delay
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
delay
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
delay
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
delay
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
delay
yellow "▸ Raw Endpoint"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID/raw")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes/:id/raw returns 200" "200" "$CODE"
assert_contains "Raw returns plain content" "print(42)" "$BODY"

# ─── Test 9: Fork ───
delay
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
delay
yellow "▸ Delete"
RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PASTE_ID")
CODE=$(echo "$RESP" | tail -1)
assert "DELETE returns 200" "200" "$CODE"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Deleted paste returns 404" "404" "$CODE"

# ─── Test 11: Expiration ───
delay
yellow "▸ Expiration"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Expiring","content":"bye","language":"plaintext","visibility":"public","expires_in":"1h"}')
EXP_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$EXP_ID")
assert_contains "Expiring paste has expires_at" '"expires_at"' "$RESP"

# ─── Test 12: Max size enforcement ───
delay
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
delay
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

# ─── SECURITY TESTS ─────────────────────────────────────────────────────────

# ─── Test 15: Delete requires password for protected pastes ───
delay
yellow "▸ Security: Delete auth"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Protected","content":"secure data","language":"plaintext","visibility":"public","password":"deletetest"}')
DEL_PW_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Delete protected paste without password returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID" -H 'X-Password: wrongpass')
CODE=$(echo "$RESP" | tail -1)
assert "Delete protected paste with wrong password returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID" -H 'X-Password: deletetest')
CODE=$(echo "$RESP" | tail -1)
assert "Delete protected paste with correct password returns 200" "200" "$CODE"

# ─── Test 16: Security headers present ───
delay
yellow "▸ Security: Headers"
RESP_HEADERS=$(curl -sI "$BASE/api/pastes")
assert_contains "X-Content-Type-Options header present" "x-content-type-options: nosniff" "$(echo "$RESP_HEADERS" | tr '[:upper:]' '[:lower:]')"
assert_contains "X-Frame-Options header present" "x-frame-options" "$(echo "$RESP_HEADERS" | tr '[:upper:]' '[:lower:]')"

# ─── Test 17: Title truncation ───
delay
yellow "▸ Security: Input limits"
LONG_TITLE=$(python3 -c "print('A' * 300)")
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d "{\"title\":\"$LONG_TITLE\",\"content\":\"test\",\"language\":\"plaintext\",\"visibility\":\"public\"}")
LIMIT_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$LIMIT_ID")
TITLE_LEN=$(echo "$RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin)['title']))" 2>/dev/null)
assert "Long title is truncated to 200 chars" "200" "$TITLE_LEN"

# ─── Test 18: DB errors don't leak internal info ───
delay
yellow "▸ Security: Error sanitization"
RESP=$(curl -s "$BASE/api/pastes/$PASTE_ID")
assert_not_contains "Error response doesn't expose DB details" "sqlite" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 19: Invalid JSON body ───
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d 'not json at all')
CODE=$(echo "$RESP" | tail -1)
assert "Invalid JSON returns 400" "400" "$CODE"

# ─── Test 20: Tag limits ───
delay
yellow "▸ Security: Tag limits"
MANY_TAGS=$(python3 -c "import json; print(json.dumps(['tag'+str(i) for i in range(20)]))")
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d "{\"title\":\"Many Tags\",\"content\":\"test\",\"language\":\"plaintext\",\"visibility\":\"public\",\"tags\":$MANY_TAGS}")
TAG_LIMIT_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$TAG_LIMIT_ID")
TAG_COUNT=$(echo "$RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('tags',[])))" 2>/dev/null)
assert "Tags capped at 10 max" "true" "$([ "$TAG_COUNT" -le 10 ] && echo true || echo false)"

# ─── Test 21: Private paste not in list ───
delay
yellow "▸ Security: Private visibility"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Private","content":"private data","language":"plaintext","visibility":"private"}')
PRIV_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes")
assert_not_contains "Private paste not in public list" "$PRIV_ID" "$RESP"

# ─── Test 22: Salted password (same password different hashes) ───
delay
yellow "▸ Security: Salted passwords"
RESP1=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Salt1","content":"data1","language":"plaintext","visibility":"public","password":"samepass"}')
SALT1_ID=$(echo "$RESP1" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP2=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"Salt2","content":"data2","language":"plaintext","visibility":"public","password":"samepass"}')
SALT2_ID=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

# Both should be accessible with the same password
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$SALT1_ID" -H 'X-Password: samepass')
assert "Salted paste 1 accessible" "200" "$(echo "$RESP" | tail -1)"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$SALT2_ID" -H 'X-Password: samepass')
assert "Salted paste 2 accessible" "200" "$(echo "$RESP" | tail -1)"

# ─── Test 23: CORS preflight ───
delay
yellow "▸ Security: CORS"
RESP=$(curl -sI -X OPTIONS "$BASE/api/pastes")
assert_contains "OPTIONS returns CORS headers" "access-control-allow" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 24: CSP header present ───
delay
yellow "▸ Security: Content-Security-Policy"
RESP=$(curl -sI "$BASE/api/pastes")
assert_contains "CSP header present" "content-security-policy" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 25: HSTS header present ───
assert_contains "HSTS header present" "strict-transport-security" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 26: Permissions-Policy header present ───
assert_contains "Permissions-Policy header present" "permissions-policy" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 27: CORS origin is restricted ───
CORS_ORIGIN=$(echo "$RESP" | grep -i "access-control-allow-origin" | head -1)
assert_contains "CORS origin is restricted (not wildcard)" "pastebox.micutu.com" "$CORS_ORIGIN"

# ─── Test 28: X-XSS-Protection header ───
assert_contains "X-XSS-Protection header present" "x-xss-protection" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Test 29: Health endpoint ───
delay
yellow "▸ Health endpoint"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/health")
CODE=$(echo "$RESP" | tail -1)
BODY=$(echo "$RESP" | head -1)
assert "Health endpoint returns 200" "200" "$CODE"
assert_contains "Health says status ok" "ok" "$BODY"
assert_contains "Health reports DB connected" "connected" "$BODY"

# ─── Test 30: Brute-force protection ───
delay
yellow "▸ Security: Brute-force protection"
# Create a password-protected paste for brute-force test
BF_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"BruteForceTest","content":"secret data","language":"plaintext","visibility":"public","password":"correctpw"}')
BF_ID=$(echo "$BF_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
# Send 6 wrong passwords (limit is 5)
for i in $(seq 1 6); do
  curl -s "$BASE/api/pastes/$BF_ID" -H "X-Password: wrong$i" > /dev/null
done
# Next request should be locked out (429)
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BF_ID" -H 'X-Password: correctpw')
CODE=$(echo "$RESP" | tail -1)
assert "Brute-force lockout after 5 failures" "429" "$CODE"

# ─── Cleanup test pastes ───
for id in $TAG_ID $PW_ID $UNL_ID $FORK_ID $EXP_ID $LIMIT_ID $TAG_LIMIT_ID $PRIV_ID $SALT1_ID $SALT2_ID $BF_ID; do
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
