#!/bin/bash
# PasteBox Backend Integration Tests
# Run against a live server (default http://localhost:7777)

BASE="${1:-http://localhost:7777}"
PASS=0
FAIL=0
TOTAL=0

# CSRF header required for all state-changing requests
CSRF='-H X-Requested-With:PasteBox'

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
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
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

# ─── Test 3: List pastes (paginated response) ───
delay
yellow "▸ List Pastes"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes returns 200" "200" "$CODE"
assert_contains "List contains our paste" "$PASTE_ID" "$BODY"
assert_contains "Response has page field" '"page"' "$BODY"
assert_contains "Response has data array" '"data"' "$BODY"

# ─── Test 4: Pagination ───
delay
yellow "▸ Pagination"
RESP=$(curl -s "$BASE/api/pastes?page=1&limit=2")
PAGE=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('page',0))" 2>/dev/null)
COUNT=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('count',0))" 2>/dev/null)
HAS_MORE=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('has_more',False))" 2>/dev/null)
assert "Pagination returns page 1" "1" "$PAGE"
assert "Pagination returns max 2 items" "true" "$([ "$COUNT" -le 2 ] && echo true || echo false)"
RESP2=$(curl -s "$BASE/api/pastes?page=999&limit=2")
COUNT2=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin).get('count',0))" 2>/dev/null)
assert "High page returns 0 items" "0" "$COUNT2"

# ─── Test 5: Tags and filter ───
delay
yellow "▸ Tags"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Tagged","content":"test","language":"plaintext","visibility":"public","tags":["alpha","beta"]}')
TAG_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes?tag=alpha")
assert_contains "Tag filter returns tagged paste" "$TAG_ID" "$RESP"
RESP=$(curl -s "$BASE/api/pastes?tag=nonexistent999")
assert_not_contains "Tag filter excludes unmatched" "$TAG_ID" "$RESP"

# ─── Test 6: Password protection (returns 404 for enumeration prevention) ───
delay
yellow "▸ Password Protection"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Secret","content":"hidden data","language":"plaintext","visibility":"public","password":"mypass123"}')
PW_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Protected paste returns 404 without password" "404" "$CODE"
assert_contains "Response says password required" '"password_required":true' "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID" -H 'X-Password: wrongpass')
CODE=$(echo "$RESP" | tail -1)
assert "Wrong password returns 404 (anti-enumeration)" "404" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PW_ID" -H 'X-Password: mypass123')
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "Correct password returns 200" "200" "$CODE"
assert_contains "Returns content with correct password" '"content":"hidden data"' "$BODY"

# ─── Test 7: Password validation ───
delay
yellow "▸ Password Validation"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Short PW","content":"test","language":"plaintext","password":"ab"}')
CODE=$(echo "$RESP" | tail -1)
assert "Short password (< 4 chars) rejected" "400" "$CODE"
assert_contains "Short password error message" "at least 4" "$(echo "$RESP" | head -n -1)"

LONG_PW=$(python3 -c "print('x' * 300)")
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d "{\"title\":\"Long PW\",\"content\":\"test\",\"language\":\"plaintext\",\"password\":\"$LONG_PW\"}")
CODE=$(echo "$RESP" | tail -1)
assert "Long password (> 256 chars) rejected" "400" "$CODE"

# ─── Test 8: Burn after read ───
delay
yellow "▸ Burn After Read"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Burn Me","content":"ephemeral","language":"plaintext","visibility":"public","burn_after_read":true}')
BURN_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
CODE=$(echo "$RESP" | tail -1)
assert "First read of burn paste returns 200" "200" "$CODE"
assert_contains "First read has burned flag" '"burned":true' "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BURN_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Second read of burned paste returns 404" "404" "$CODE"

# ─── Test 9: Visibility ───
delay
yellow "▸ Visibility"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Unlisted","content":"hidden","language":"plaintext","visibility":"unlisted"}')
UNL_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes")
assert_not_contains "Unlisted paste not in public list" "$UNL_ID" "$RESP"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$UNL_ID")
assert "Unlisted paste accessible via direct link" "200" "$(echo "$RESP" | tail -1)"

# ─── Test 10: Raw endpoint with Content-Disposition ───
delay
yellow "▸ Raw Endpoint"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$UNL_ID/raw")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "GET /api/pastes/:id/raw returns 200" "200" "$CODE"
assert_contains "Raw returns plain content" "hidden" "$BODY"
HEADERS=$(curl -sI "$BASE/api/pastes/$UNL_ID/raw")
assert_contains "Raw has Content-Disposition: inline" "content-disposition: inline" "$(echo "$HEADERS" | tr '[:upper:]' '[:lower:]')"

# ─── Test 11: Fork ───
delay
yellow "▸ Fork"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes/$UNL_ID/fork" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' -d '{}')
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -1)
assert "POST /api/pastes/:id/fork returns 201" "201" "$CODE"
FORK_ID=$(echo "$BODY" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$FORK_ID")
assert_contains "Forked paste has parent_id" "\"parent_id\":\"$UNL_ID\"" "$RESP"

# ─── Test 12: Update (PUT) endpoint ───
delay
yellow "▸ Update Paste"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Original","content":"original content","language":"python","visibility":"public"}')
UPD_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$UPD_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Updated Title","content":"updated content","language":"javascript","visibility":"unlisted"}')
CODE=$(echo "$RESP" | tail -1)
assert "PUT /api/pastes/:id returns 200" "200" "$CODE"
assert_contains "Update response has message" '"Paste updated"' "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s "$BASE/api/pastes/$UPD_ID")
assert_contains "Updated title persisted" '"title":"Updated Title"' "$RESP"
assert_contains "Updated content persisted" '"content":"updated content"' "$RESP"
assert_contains "Updated language persisted" '"language":"javascript"' "$RESP"

# ─── Test 13: Update with tags ───
delay
RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$UPD_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"tags":["newtag1","newtag2"]}')
CODE=$(echo "$RESP" | tail -1)
assert "PUT with tags returns 200" "200" "$CODE"
RESP=$(curl -s "$BASE/api/pastes/$UPD_ID")
assert_contains "Updated tags contain newtag1" "newtag1" "$RESP"
assert_contains "Updated tags contain newtag2" "newtag2" "$RESP"

# ─── Test 14: Update nonexistent paste ───
RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/DOESNOTEXIST999" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"nope"}')
CODE=$(echo "$RESP" | tail -1)
assert "PUT nonexistent paste returns 404" "404" "$CODE"

# ─── Test 15: Update password-protected paste ───
delay
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"PW Edit","content":"secret","language":"plaintext","password":"editpw"}')
PW_UPD_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$PW_UPD_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"hacked"}')
CODE=$(echo "$RESP" | tail -1)
assert "Update protected paste without password returns 404" "404" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$PW_UPD_ID" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' -H 'X-Password: editpw' \
  -d '{"title":"Authorized Edit"}')
CODE=$(echo "$RESP" | tail -1)
assert "Update protected paste with correct password returns 200" "200" "$CODE"

# ─── Test 16: Delete ───
delay
yellow "▸ Delete"
RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$PASTE_ID" \
  -H 'X-Requested-With: PasteBox')
CODE=$(echo "$RESP" | tail -1)
assert "DELETE returns 200" "200" "$CODE"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$PASTE_ID")
CODE=$(echo "$RESP" | tail -1)
assert "Deleted paste returns 404" "404" "$CODE"

# ─── Test 17: Delete requires password for protected pastes ───
delay
yellow "▸ Delete auth"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Protected","content":"secure data","language":"plaintext","visibility":"public","password":"deletetest"}')
DEL_PW_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID" \
  -H 'X-Requested-With: PasteBox')
CODE=$(echo "$RESP" | tail -1)
assert "Delete protected paste without password returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID" \
  -H 'X-Requested-With: PasteBox' -H 'X-Password: wrongpass')
CODE=$(echo "$RESP" | tail -1)
assert "Delete with wrong password returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$DEL_PW_ID" \
  -H 'X-Requested-With: PasteBox' -H 'X-Password: deletetest')
CODE=$(echo "$RESP" | tail -1)
assert "Delete with correct password returns 200" "200" "$CODE"

# ─── Test 18: Expiration ───
delay
yellow "▸ Expiration"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Expiring","content":"bye","language":"plaintext","visibility":"public","expires_in":"1h"}')
EXP_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$EXP_ID")
assert_contains "Expiring paste has expires_at" '"expires_at"' "$RESP"

# ─── Test 19: Max size enforcement ───
delay
yellow "▸ Max Size"
python3 -c "
import json, sys
data = {'title':'Big','content':'x'*600000,'language':'plaintext','visibility':'public'}
sys.stdout.write(json.dumps(data))
" > /tmp/pastebox_bigtest.json
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d @/tmp/pastebox_bigtest.json)
CODE=$(echo "$RESP" | tail -1)
assert "Oversized paste rejected (400 or 413)" "true" "$([ "$CODE" = "400" ] || [ "$CODE" = "413" ] && echo true || echo false)"
rm -f /tmp/pastebox_bigtest.json

# ─── Test 20: Empty content rejected ───
delay
yellow "▸ Validation"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Empty","content":"","language":"plaintext","visibility":"public"}')
CODE=$(echo "$RESP" | tail -1)
assert "Empty content rejected (400)" "400" "$CODE"

# ─── Test 21: 404 for nonexistent paste ───
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/DOESNOTEXIST999")
CODE=$(echo "$RESP" | tail -1)
assert "Nonexistent paste returns 404" "404" "$CODE"

# ─── Test 22: Invalid JSON body ───
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d 'not json at all')
CODE=$(echo "$RESP" | tail -1)
assert "Invalid JSON returns 400" "400" "$CODE"

# ═══════════════════════════════════════════════════════════════════════════
# CSRF PROTECTION TESTS
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: CSRF Protection"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' \
  -d '{"title":"No CSRF","content":"test","language":"plaintext"}')
CODE=$(echo "$RESP" | tail -1)
assert "POST without X-Requested-With returns 403" "403" "$CODE"
assert_contains "CSRF error message" "Missing required header" "$(echo "$RESP" | head -n -1)"

RESP=$(curl -s -w "\n%{http_code}" -X PUT "$BASE/api/pastes/$UPD_ID" \
  -H 'Content-Type: application/json' \
  -d '{"title":"No CSRF"}')
CODE=$(echo "$RESP" | tail -1)
assert "PUT without X-Requested-With returns 403" "403" "$CODE"

RESP=$(curl -s -w "\n%{http_code}" -X DELETE "$BASE/api/pastes/$UPD_ID")
CODE=$(echo "$RESP" | tail -1)
assert "DELETE without X-Requested-With returns 403" "403" "$CODE"

# Fork also needs CSRF
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/api/pastes/$UPD_ID/fork" \
  -H 'Content-Type: application/json' -d '{}')
CODE=$(echo "$RESP" | tail -1)
assert "Fork without X-Requested-With returns 403" "403" "$CODE"

# ═══════════════════════════════════════════════════════════════════════════
# SECURITY HEADERS TESTS
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: Headers"
RESP_HEADERS=$(curl -sI "$BASE/api/pastes")
RESP_LC=$(echo "$RESP_HEADERS" | tr '[:upper:]' '[:lower:]')
assert_contains "X-Content-Type-Options header present" "x-content-type-options: nosniff" "$RESP_LC"
assert_contains "X-Frame-Options header present" "x-frame-options" "$RESP_LC"
assert_contains "CSP header present" "content-security-policy" "$RESP_LC"
assert_contains "HSTS header present" "strict-transport-security" "$RESP_LC"
assert_contains "Permissions-Policy header present" "permissions-policy" "$RESP_LC"
assert_contains "X-XSS-Protection header present" "x-xss-protection" "$RESP_LC"
assert_contains "Referrer-Policy header present" "referrer-policy" "$RESP_LC"

# ─── CORS ───
CORS_ORIGIN=$(echo "$RESP_HEADERS" | grep -i "access-control-allow-origin" | head -1)
assert_contains "CORS origin is restricted (not wildcard)" "pastebox.micutu.com" "$CORS_ORIGIN"
assert_not_contains "CORS origin is not wildcard" "\\*" "$CORS_ORIGIN"

CORS_OPTS=$(curl -sI -X OPTIONS "$BASE/api/pastes" | tr '[:upper:]' '[:lower:]')
assert_contains "OPTIONS returns CORS headers" "access-control-allow" "$CORS_OPTS"
assert_contains "CORS allows PUT method" "put" "$CORS_OPTS"
assert_contains "CORS allows X-Requested-With header" "x-requested-with" "$CORS_OPTS"

# ─── ETag ───
delay
yellow "▸ Performance: ETag"
ETAG_RESP=$(curl -sI "$BASE/api/pastes/$UPD_ID")
ETAG=$(echo "$ETAG_RESP" | grep -i "etag" | head -1 | tr -d '\r')
assert_contains "ETag header present on paste" "etag" "$(echo "$ETAG" | tr '[:upper:]' '[:lower:]')"
assert_contains "Cache-Control present" "cache-control" "$(echo "$ETAG_RESP" | tr '[:upper:]' '[:lower:]')"

# ─── Title truncation ───
delay
yellow "▸ Security: Input limits"
LONG_TITLE=$(python3 -c "print('A' * 300)")
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d "{\"title\":\"$LONG_TITLE\",\"content\":\"test\",\"language\":\"plaintext\",\"visibility\":\"public\"}")
LIMIT_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$LIMIT_ID")
TITLE_LEN=$(echo "$RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin)['title']))" 2>/dev/null)
assert "Long title is truncated to 200 chars" "200" "$TITLE_LEN"

# ─── Tag limits ───
delay
yellow "▸ Security: Tag limits"
MANY_TAGS=$(python3 -c "import json; print(json.dumps(['tag'+str(i) for i in range(20)]))")
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d "{\"title\":\"Many Tags\",\"content\":\"test\",\"language\":\"plaintext\",\"visibility\":\"public\",\"tags\":$MANY_TAGS}")
TAG_LIMIT_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$TAG_LIMIT_ID")
TAG_COUNT=$(echo "$RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('tags',[])))" 2>/dev/null)
assert "Tags capped at 10 max" "true" "$([ "$TAG_COUNT" -le 10 ] && echo true || echo false)"

# ─── Private visibility ───
delay
yellow "▸ Security: Private visibility"
RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Private","content":"private data","language":"plaintext","visibility":"private"}')
PRIV_ID=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes")
assert_not_contains "Private paste not in public list" "$PRIV_ID" "$RESP"

# ─── Salted passwords ───
delay
yellow "▸ Security: Salted passwords"
RESP1=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Salt1","content":"data1","language":"plaintext","visibility":"public","password":"samepass"}')
SALT1_ID=$(echo "$RESP1" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP2=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"Salt2","content":"data2","language":"plaintext","visibility":"public","password":"samepass"}')
SALT2_ID=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$SALT1_ID" -H 'X-Password: samepass')
assert "Salted paste 1 accessible" "200" "$(echo "$RESP" | tail -1)"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$SALT2_ID" -H 'X-Password: samepass')
assert "Salted paste 2 accessible" "200" "$(echo "$RESP" | tail -1)"

# ─── Error sanitization ───
delay
yellow "▸ Security: Error sanitization"
RESP=$(curl -s "$BASE/api/pastes/DOESNOTEXIST999")
assert_not_contains "Error doesn't expose DB details" "sqlite" "$(echo "$RESP" | tr '[:upper:]' '[:lower:]')"
assert_not_contains "Error doesn't expose stack traces" "at " "$RESP"

# ─── Health endpoint ───
delay
yellow "▸ Health endpoint"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/health")
CODE=$(echo "$RESP" | tail -1)
BODY=$(echo "$RESP" | head -1)
assert "Health endpoint returns 200" "200" "$CODE"
assert_contains "Health says status ok" "ok" "$BODY"
assert_contains "Health reports DB connected" "connected" "$BODY"

# ═══════════════════════════════════════════════════════════════════════════
# XSS INJECTION TESTS
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: XSS Injection"
XSS_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"XSS Test","content":"<script>alert(1)<\/script>","language":"plaintext","visibility":"public"}')
XSS_ID=$(echo "$XSS_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
RESP=$(curl -s "$BASE/api/pastes/$XSS_ID")
assert_contains "XSS content stored safely as data" "alert" "$RESP"
HEADERS=$(curl -sI "$BASE/api/pastes/$XSS_ID")
assert_contains "CSP blocks inline scripts" "script-src" "$(echo "$HEADERS" | tr '[:upper:]' '[:lower:]')"

# ═══════════════════════════════════════════════════════════════════════════
# SQL INJECTION TESTS
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: SQL Injection"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/1%27%20OR%20%271%27%3D%271")
CODE=$(echo "$RESP" | tail -1)
assert "SQL injection in ID returns 404 or handled safely" "true" "$([ "$CODE" = "404" ] || [ "$CODE" = "429" ] && echo true || echo false)"

RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes?tag=test%27%20OR%201%3D1%20--")
CODE=$(echo "$RESP" | tail -1)
assert "SQL injection in tag parameter returns 200 (safe)" "200" "$CODE"

# ═══════════════════════════════════════════════════════════════════════════
# ID UNIQUENESS TEST
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: ID Uniqueness"
IDS=""
for i in $(seq 1 10); do
  R=$(curl -s -X POST "$BASE/api/pastes" \
    -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
    -d '{"title":"Unique","content":"test","language":"plaintext","visibility":"public"}')
  ID=$(echo "$R" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
  IDS="$IDS $ID"
done
UNIQUE_COUNT=$(echo "$IDS" | tr ' ' '\n' | sort -u | grep -c .)
assert "10 pastes have 10 unique IDs" "10" "$UNIQUE_COUNT"

# ═══════════════════════════════════════════════════════════════════════════
# BRUTE-FORCE PROTECTION
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Security: Brute-force protection"
BF_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"BruteForceTest","content":"secret data","language":"plaintext","visibility":"public","password":"correctpw"}')
BF_ID=$(echo "$BF_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)
for i in $(seq 1 6); do
  curl -s "$BASE/api/pastes/$BF_ID" -H "X-Password: wrong$i" > /dev/null
done
RESP=$(curl -s -w "\n%{http_code}" "$BASE/api/pastes/$BF_ID" -H 'X-Password: correctpw')
CODE=$(echo "$RESP" | tail -1)
assert "Brute-force lockout after 5 failures" "429" "$CODE"
assert_contains "Retry-After header on lockout" "retry-after" "$(curl -sI "$BASE/api/pastes/$BF_ID" -H 'X-Password: correctpw' | tr '[:upper:]' '[:lower:]')"

# ═══════════════════════════════════════════════════════════════════════════
# LOAD / CONCURRENCY TEST
# ═══════════════════════════════════════════════════════════════════════════
delay
yellow "▸ Performance: Concurrent requests"
# Create a paste to load test against
LOAD_RESP=$(curl -s -X POST "$BASE/api/pastes" \
  -H 'Content-Type: application/json' -H 'X-Requested-With: PasteBox' \
  -d '{"title":"LoadTest","content":"load test content","language":"plaintext","visibility":"public"}')
LOAD_ID=$(echo "$LOAD_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])" 2>/dev/null)

# Fire 20 concurrent GET requests
START=$(date +%s%N)
for i in $(seq 1 20); do
  curl -s "$BASE/api/pastes/$LOAD_ID" > /dev/null &
done
wait
END=$(date +%s%N)
ELAPSED_MS=$(( (END - START) / 1000000 ))
assert "20 concurrent GETs complete in < 5000ms" "true" "$([ "$ELAPSED_MS" -lt 5000 ] && echo true || echo false)"

# ─── Response time benchmark ───
delay
START=$(date +%s%N)
curl -s "$BASE/api/pastes/$LOAD_ID" > /dev/null
END=$(date +%s%N)
SINGLE_MS=$(( (END - START) / 1000000 ))
assert "Single GET response < 200ms" "true" "$([ "$SINGLE_MS" -lt 200 ] && echo true || echo false)"

START=$(date +%s%N)
curl -s "$BASE/api/pastes" > /dev/null
END=$(date +%s%N)
LIST_MS=$(( (END - START) / 1000000 ))
assert "List pastes response < 300ms" "true" "$([ "$LIST_MS" -lt 300 ] && echo true || echo false)"

# ─── Cleanup test pastes ───
for id in $TAG_ID $PW_ID $UNL_ID $FORK_ID $EXP_ID $LIMIT_ID $TAG_LIMIT_ID $PRIV_ID $SALT1_ID $SALT2_ID $BF_ID $UPD_ID $PW_UPD_ID $XSS_ID $LOAD_ID; do
  curl -s -X DELETE "$BASE/api/pastes/$id" -H 'X-Requested-With: PasteBox' > /dev/null 2>&1
done
# Cleanup uniqueness test IDs
for id in $IDS; do
  curl -s -X DELETE "$BASE/api/pastes/$id" -H 'X-Requested-With: PasteBox' > /dev/null 2>&1
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
