use std::path::PathBuf;
use std::path::Path;
use std::fs;
use std::fs::remove_file;
use std::time::Duration;
use std::thread;

use anyhow::{Context, Result};
use clap::{ArgAction, Parser};
use indicatif::{ProgressBar, ProgressStyle};

use log_processor::ingest_files;

/// Ingest `.tar.gz` CAN/telemetry log archives into InfluxDB 2.x
#[derive(Debug, Parser)]
struct Opts {
    /// .tar.gz files or glob patterns (e.g. logs/*.tar.gz)
    #[arg(required = true)]
    inputs: Vec<String>,

    /// InfluxDB URL (e.g. http://localhost:8086)
    #[arg(long)]
    url: String,

    /// InfluxDB API Token
    #[arg(long)]
    token: String,

    /// InfluxDB org
    #[arg(long)]
    org: String,

    /// InfluxDB bucket
    #[arg(long)]
    bucket: String,

    /// Batch size for Influx writes
    #[arg(long, default_value_t = 5_000)]
    batch_size: usize,

    /// Print matched files and exit
    #[arg(long, action = ArgAction::SetTrue)]
    dry_run: bool,

    /// Delete files once extracted
    #[arg(long, action = ArgAction::SetTrue)]
    delete: bool,
}

/// Expand patterns like "dir/*.ext" without using the `glob` crate.
/// - Supports `*` in the file name portion only (no recursion / no `**`).
/// - If a pattern has no `*`, it's treated as a literal path (included if it exists).
pub fn expand_inputs(patterns: &[String]) -> Result<Vec<PathBuf>> {
    let mut files = Vec::new();

    for pat in patterns {
        if !pat.contains('*') {
            let p = Path::new(pat);
            if p.exists() {
                files.push(p.to_path_buf());
            }
            continue;
        }

        let (dir, file_pat) = split_dir_and_basename_pattern(pat)?;
        let entries = fs::read_dir(&dir)
            .with_context(|| format!("reading directory {}", dir.display()))?;

        for ent in entries {
            let ent = ent?;
            let name = match ent.file_name().into_string() {
                Ok(s) => s,
                Err(_) => continue, // skip invalid UTF-8 names
            };
            if matches_star_only(&file_pat, &name) {
                files.push(ent.path());
            }
        }
    }

    files.sort();
    files.dedup();
    Ok(files)
}

/// Split "some/dir/*.ext" into ("some/dir", "*.ext").
/// If there's no parent, uses "." as the directory.
fn split_dir_and_basename_pattern(pat: &str) -> Result<(PathBuf, String)> {
    let path = Path::new(pat);
    let dir = path.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
    let basename = path
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| anyhow::anyhow!("invalid pattern (missing basename): {}", pat))?
        .to_string();
    Ok((dir, basename))
}

/// Simple wildcard matcher supporting only `*` (matches any sequence, including empty).
/// No character escaping; path separators are not special here since we only match basenames.
fn matches_star_only(pattern: &str, text: &str) -> bool {
    // Fast path: no `*` means exact match.
    if !pattern.contains('*') {
        return pattern == text;
    }

    // Two-pointer greedy matcher for `*` only.
    let (p_bytes, t_bytes) = (pattern.as_bytes(), text.as_bytes());
    let (mut pi, mut ti) = (0usize, 0usize);
    let (mut star_idx, mut match_after_star) = (None, 0usize);

    while ti < t_bytes.len() {
        if pi < p_bytes.len() && (p_bytes[pi] == t_bytes[ti]) {
            // literal char match
            pi += 1;
            ti += 1;
        } else if pi < p_bytes.len() && p_bytes[pi] == b'*' {
            // record star, advance pattern, try to match zero chars first
            star_idx = Some(pi);
            pi += 1;
            match_after_star = ti;
        } else if let Some(si) = star_idx {
            // backtrack: let star consume one more char
            pi = si + 1;
            match_after_star += 1;
            ti = match_after_star;
        } else {
            return false;
        }
    }

    // Consume trailing stars
    while pi < p_bytes.len() && p_bytes[pi] == b'*' {
        pi += 1;
    }

    pi == p_bytes.len()
}

#[tokio::main]
async fn main() -> Result<()> {
    let opts = Opts::parse();

    loop {
        let files = expand_inputs(&opts.inputs)?;
        if files.is_empty() {
            thread::sleep(Duration::from_secs(60));
        }

        if opts.dry_run {
            for f in &files {
                println!("{}", f.display());
            }
            return Ok(());
        }

        let pb = ProgressBar::new(files.len() as u64);
        pb.set_style(
            ProgressStyle::with_template(
                "{spinner:.green} [{elapsed_precise}] [{wide_bar}] {pos}/{len} {msg}",
            )
            .unwrap(),
        );

        let (ok, bad, skipped, missing) = ingest_files(
            &opts.url,
            &opts.token,
            &opts.org,
            &opts.bucket,
            &files,
            opts.batch_size,
            opts.delete,
        )
        .await?;

        pb.finish_and_clear();
        println!(
            "Done. wrote_points={ok} bad_records={bad} files_matched={} files_skipped={} files_missing={}",
            files.len(),
            skipped,
            missing
        );
        if ok == 0 && skipped > 0 {
            thread::sleep(Duration::from_secs(60));
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::matches_star_only;

    #[test]
    fn test_matches_star_only() {
        assert!(matches_star_only("*.rs", "main.rs"));
        assert!(matches_star_only("file.*", "file.txt"));
        assert!(matches_star_only("*", "anything"));
        assert!(matches_star_only("a*b*c", "axxxbxxc"));
        assert!(matches_star_only("no_star", "no_star"));
        assert!(!matches_star_only("no_star", "nope"));
        assert!(!matches_star_only("*.rs", "main.r"));
        assert!(matches_star_only("foo*bar", "foobar"));
        assert!(matches_star_only("foo*bar", "foo___bar"));
        assert!(matches_star_only("foo*bar*", "foo_bar_baz"));
        assert!(matches_star_only("*end", "the_end"));
        assert!(!matches_star_only("*end", "the_end_"));
        assert!(matches_star_only("prefix*", "prefix"));
    }
}
