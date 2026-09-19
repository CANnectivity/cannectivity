#!/usr/bin/env bash
set -e

STAGE_NAME="$1"
STATUS="$2"
HEADER_TAG="<!-- ci-stage-status-comment -->"

if [ -z "$CI_MERGE_REQUEST_IID" ]; then
  echo "Not running in a Merge Request pipeline. Skipping."
  exit 0
fi

glab config set skip_tls_verify true
# 1. Fetch notes safely
NOTES_JSON=$(glab api "projects/:id/merge_requests/${CI_MERGE_REQUEST_IID}/notes?per_page=100" 2>/dev/null || echo "[]")

# 2. Extract ONLY numeric Note IDs matching the header tag
EXISTING_NOTE_ID=$(echo "$NOTES_JSON" | jq -r ".[]? | select(.body? | contains(\"${HEADER_TAG}\")) | .id" | grep -E '^[0-9]+$' | head -n 1 || true)

BODY_CONTENT="${HEADER_TAG}
### 🚀 CI Pipeline Status Summary

| Stage | Status | Updated At |
| :--- | :--- | :--- |
| **${STAGE_NAME}** | ${STATUS} | $(date -u +'%Y-%m-%d %H:%M:%S UTC') |
"

# 3. Create or Update conditionally
if [ -n "$EXISTING_NOTE_ID" ]; then
  echo "Updating existing note ID: ${EXISTING_NOTE_ID}"
  glab api -X PUT "projects/:id/merge_requests/${CI_MERGE_REQUEST_IID}/notes/${EXISTING_NOTE_ID}" \
    -f "body=${BODY_CONTENT}" > /dev/null
else
  echo "Creating new comment on MR !${CI_MERGE_REQUEST_IID}"
  glab mr note "${CI_MERGE_REQUEST_IID}" -m "${BODY_CONTENT}" > /dev/null
fi
