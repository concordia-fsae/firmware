#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile
import urllib.parse
import urllib.request
import zipfile
from urllib.error import HTTPError

API_ROOT = "https://api.github.com"


def eprint(*args):
    print(*args, file=sys.stderr)


def read_http_error(err):
    body = ""
    try:
        body = err.read().decode("utf-8", errors="replace")
    except Exception:
        body = ""
    return body


def api_request(path, token):
    url = f"{API_ROOT}{path}"
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github+json",
            "User-Agent": "artifact-diff-script",
        },
    )
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode("utf-8"))


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def _open_no_redirect(req):
    opener = urllib.request.build_opener(_NoRedirect())
    return opener.open(req)


def api_request_raw(url, token):
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github+json",
            "User-Agent": "artifact-diff-script",
        },
    )
    return _open_no_redirect(req)


def download_artifact_zip(repo, run_id, artifact_name, token):
    data = api_request(f"/repos/{repo}/actions/runs/{run_id}/artifacts", token)
    artifacts = data.get("artifacts", [])
    for artifact in artifacts:
        if artifact.get("name") == artifact_name:
            url = artifact.get("archive_download_url")
            if not url:
                return None
            fd, path = tempfile.mkstemp(prefix="artifact-", suffix=".zip")
            os.close(fd)
            req = urllib.request.Request(
                url,
                headers={
                    "Authorization": f"token {token}",
                    "Accept": "application/vnd.github+json",
                    "User-Agent": "artifact-diff-script",
                },
            )
            try:
                with _open_no_redirect(req) as resp:
                    if resp.status in (301, 302, 303, 307, 308):
                        redirect = resp.headers.get("Location")
                    else:
                        with open(path, "wb") as out:
                            shutil.copyfileobj(resp, out)
                        return path
            except urllib.error.HTTPError as err:
                if err.code in (301, 302, 303, 307, 308):
                    redirect = err.headers.get("Location")
                else:
                    raise

            if not redirect:
                return None

            download_req = urllib.request.Request(
                redirect,
                headers={"User-Agent": "artifact-diff-script"},
            )
            with urllib.request.urlopen(download_req) as resp, open(path, "wb") as out:
                shutil.copyfileobj(resp, out)
            return path
    return None


def hash_zip(path):
    results = {}
    with zipfile.ZipFile(path, "r") as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            with zf.open(info, "r") as fp:
                digest = hashlib.sha256()
                while True:
                    chunk = fp.read(1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
            results[info.filename] = digest.hexdigest()
    return results


def truncate_list(items, max_items):
    if max_items <= 0:
        return items, 0
    if len(items) <= max_items:
        return items, 0
    return items[:max_items], len(items) - max_items


def render_table(rows, headers):
    if not rows:
        return "_None_\n"
    lines = ["| " + " | ".join(headers) + " |", "| " + " | ".join(["---"] * len(headers)) + " |"]
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines) + "\n"


def build_comment(artifact_name, base_run_id, pr_run_id, added, deleted, changed, unchanged_count, max_items, repo, tag):
    added_rows = [[f"`{name}`", f"`{sha}`"] for name, sha in added]
    deleted_rows = [[f"`{name}`", f"`{sha}`"] for name, sha in deleted]
    changed_rows = [[f"`{name}`", f"`{old}`", f"`{new}`"] for name, old, new in changed]

    added_rows, added_trim = truncate_list(added_rows, max_items)
    deleted_rows, deleted_trim = truncate_list(deleted_rows, max_items)
    changed_rows, changed_trim = truncate_list(changed_rows, max_items)

    base_link = f"https://github.com/{repo}/actions/runs/{base_run_id}" if base_run_id else ""
    pr_link = f"https://github.com/{repo}/actions/runs/{pr_run_id}" if pr_run_id else ""

    lines = [
        f"<!-- {tag}:{artifact_name} -->",
        f"## Firmware artifact diff: {artifact_name}",
        "",
    ]
    if base_run_id:
        lines.append(f"Base run: {base_link}")
    else:
        lines.append("Base run: _not found_")
    if pr_run_id:
        lines.append(f"PR run: {pr_link}")
    lines.append("")
    lines.append("Summary:")
    lines.append(f"- Added: {len(added)}")
    lines.append(f"- Deleted: {len(deleted)}")
    lines.append(f"- Changed: {len(changed)}")
    lines.append(f"- Unchanged: {unchanged_count}")
    lines.append("")

    lines.append("Added:")
    lines.append(render_table(added_rows, ["Path", "SHA256"]).rstrip())
    if added_trim:
        lines.append(f"_And {added_trim} more added files not shown._")
    lines.append("")

    lines.append("Deleted:")
    lines.append(render_table(deleted_rows, ["Path", "SHA256"]).rstrip())
    if deleted_trim:
        lines.append(f"_And {deleted_trim} more deleted files not shown._")
    lines.append("")

    lines.append("Changed:")
    lines.append(render_table(changed_rows, ["Path", "Base SHA256", "PR SHA256"]).rstrip())
    if changed_trim:
        lines.append(f"_And {changed_trim} more changed files not shown._")
    lines.append("")

    return "\n".join(lines).strip() + "\n"


def find_existing_comment(repo, pr_number, token, tag, artifact_name):
    page = 1
    target = f"<!-- {tag}:{artifact_name} -->"
    while True:
        data = api_request(
            f"/repos/{repo}/issues/{pr_number}/comments?per_page=100&page={page}",
            token,
        )
        if not data:
            break
        for comment in data:
            body = comment.get("body", "")
            if target in body:
                return comment.get("id")
        page += 1
    return None


def upsert_comment(repo, pr_number, token, body, tag, artifact_name):
    existing_id = find_existing_comment(repo, pr_number, token, tag, artifact_name)
    if existing_id:
        req = urllib.request.Request(
            f"{API_ROOT}/repos/{repo}/issues/comments/{existing_id}",
            data=json.dumps({"body": body}).encode("utf-8"),
            method="PATCH",
            headers={
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github+json",
                "User-Agent": "artifact-diff-script",
            },
        )
        try:
            with urllib.request.urlopen(req) as resp:
                resp.read()
        except HTTPError as err:
            error_body = read_http_error(err)
            eprint(f"Failed to update PR comment: HTTP {err.code}")
            if error_body:
                eprint(error_body)
            raise
        return "updated"

    req = urllib.request.Request(
        f"{API_ROOT}/repos/{repo}/issues/{pr_number}/comments",
        data=json.dumps({"body": body}).encode("utf-8"),
        method="POST",
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github+json",
            "User-Agent": "artifact-diff-script",
        },
    )
    try:
        with urllib.request.urlopen(req) as resp:
            resp.read()
    except HTTPError as err:
        error_body = read_http_error(err)
        eprint(f"Failed to create PR comment: HTTP {err.code}")
        if error_body:
            eprint(error_body)
        raise
    return "created"


def main():
    parser = argparse.ArgumentParser(description="Compare GitHub artifact zips and comment on PR.")
    parser.add_argument("--artifact-name", required=True)
    parser.add_argument("--base-ref", default="")
    parser.add_argument("--comment-tag", default="firmware-artifact-diff")
    parser.add_argument("--max-items", type=int, default=100)
    args = parser.parse_args()

    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        eprint("GITHUB_TOKEN is required")
        return 2

    repo = os.environ.get("GITHUB_REPOSITORY")
    run_id = os.environ.get("GITHUB_RUN_ID")
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    workflow_ref = os.environ.get("GITHUB_WORKFLOW_REF", "")

    if not repo or not run_id or not event_path:
        eprint("Missing required GitHub environment variables")
        return 2

    with open(event_path, "r", encoding="utf-8") as f:
        event = json.load(f)

    pr = event.get("pull_request")
    if not pr:
        eprint("No pull_request found in event payload; skipping")
        return 0

    pr_number = pr.get("number")
    base_ref = args.base_ref or pr.get("base", {}).get("ref")
    if not pr_number or not base_ref:
        eprint("Missing PR number or base ref")
        return 2

    workflow_file = ""
    marker = "/.github/workflows/"
    if marker in workflow_ref:
        workflow_file = workflow_ref.split(marker, 1)[1].split("@", 1)[0]

    if not workflow_file:
        eprint("Unable to determine workflow file from GITHUB_WORKFLOW_REF")
        return 2

    current_zip = download_artifact_zip(repo, run_id, args.artifact_name, token)
    if not current_zip:
        eprint(f"Artifact {args.artifact_name} not found for run {run_id}")
        return 2

    base_run_id = None
    base_zip = None
    runs = api_request(
        f"/repos/{repo}/actions/workflows/{urllib.parse.quote(workflow_file)}/runs?branch={urllib.parse.quote(base_ref)}&status=success&per_page=1",
        token,
    )
    workflow_runs = runs.get("workflow_runs", [])
    if workflow_runs:
        base_run_id = workflow_runs[0].get("id")

    if base_run_id:
        base_zip = download_artifact_zip(repo, base_run_id, args.artifact_name, token)

    if not base_zip:
        body = build_comment(
            args.artifact_name,
            base_run_id,
            run_id,
            [],
            [],
            [],
            0,
            args.max_items,
            repo,
            args.comment_tag,
        )
        upsert_comment(repo, pr_number, token, body, args.comment_tag, args.artifact_name)
        eprint("Base artifact not found; posted placeholder comment")
        return 0

    base_hashes = hash_zip(base_zip)
    pr_hashes = hash_zip(current_zip)

    added = []
    deleted = []
    changed = []

    for name, sha in sorted(pr_hashes.items()):
        if name not in base_hashes:
            added.append((name, sha))
        elif base_hashes[name] != sha:
            changed.append((name, base_hashes[name], sha))

    for name, sha in sorted(base_hashes.items()):
        if name not in pr_hashes:
            deleted.append((name, sha))

    unchanged_count = len(pr_hashes) - len(added) - len(changed)

    body = build_comment(
        args.artifact_name,
        base_run_id,
        run_id,
        added,
        deleted,
        changed,
        unchanged_count,
        args.max_items,
        repo,
        args.comment_tag,
    )

    result = upsert_comment(repo, pr_number, token, body, args.comment_tag, args.artifact_name)
    eprint(f"Comment {result} for {args.artifact_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
