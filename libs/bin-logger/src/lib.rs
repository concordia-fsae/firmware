pub mod binrecord_can;
pub use binrecord_can::{BinRecordCan, Val}; // convenient re-export

use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};

use anyhow::Result;
use chrono::Utc;
use tempfile;

/// Length prefix (u32 LE) + bincode payload.
const LEN_PREFIX: usize = 4;

/// Rolling writer for bincode-serialized `BinRecordCan`.
pub struct BinLogger {
    dir: PathBuf,
    max_bytes: u64,
    cur_path: PathBuf,
    cur: File,
    written: u64,
    seq: u64,
}

impl BinLogger {
    /// Create a logger in `dir`, starting from the highest on-disk index.
    pub fn new(dir: impl Into<PathBuf>, max_bytes: u64) -> Result<Self> {
        let dir = dir.into();
        fs::create_dir_all(&dir)?;
        let seq = highest_existing_index(&dir)?;
        let mut s = Self {
            dir: dir.clone(),
            max_bytes,
            cur_path: PathBuf::new(),
            cur: tempfile::tempfile()?, // replaced on first rotate
            written: 0,
            seq,
        };
        s.rotate()?;
        Ok(s)
    }

    fn rotate(&mut self) -> Result<()> {
        self.seq += 1;
        let ts = Utc::now().format("%Y%m%dT%H%M%S").to_string();
        self.cur_path = self.dir.join(format!("can-bridge-{}-{:06}.bin", ts, self.seq));
        self.cur = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&self.cur_path)?;
        self.written = 0;
        Ok(())
    }

    /// Write one record (length-prefixed bincode).
    pub fn write_record(&mut self, rec: &BinRecordCan) -> Result<()> {
        let bytes = bincode::serialize(rec)?;
        if self.written + (LEN_PREFIX as u64) + (bytes.len() as u64) > self.max_bytes {
            self.rotate()?;
        }
        self.cur.write_all(&(bytes.len() as u32).to_le_bytes())?;
        self.cur.write_all(&bytes)?;
        self.written += (LEN_PREFIX as u64) + (bytes.len() as u64);
        Ok(())
    }
}

/// Iterate records in a single file, invoking `f` for each record.
pub fn iterate_file<F>(path: &Path, mut f: F) -> Result<()>
where
    F: FnMut(&BinRecordCan) -> Result<()>,
{
    let mut file = File::open(path)?;
    let mut len_buf = [0u8; LEN_PREFIX];

    loop {
        // try to read a length prefix
        if let Err(e) = file.read_exact(&mut len_buf) {
            // normal end-of-file
            if e.kind() == io::ErrorKind::UnexpectedEof {
                break;
            }
            return Err(e.into());
        }
        let len = u32::from_le_bytes(len_buf) as usize;
        let mut buf = vec![0u8; len];
        file.read_exact(&mut buf)?;
        let rec: BinRecordCan = bincode::deserialize(&buf)?;
        f(&rec)?;
    }
    Ok(())
}

/// Iterate all matching `*-######.bin` files in index order.
pub fn iterate_dir<F>(dir: &Path, mut f: F) -> Result<()>
where
    F: FnMut(&Path, &BinRecordCan) -> Result<()>,
{
    let mut items: Vec<(u64, PathBuf)> = Vec::new();
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if !entry.file_type()?.is_file() {
            continue;
        }
        let path = entry.path();
        if let Some(idx) = parse_index_from_filename(&path) {
            items.push((idx, path));
        }
    }
    items.sort_by_key(|(idx, _)| *idx);

    for (_, path) in items {
        iterate_file(&path, |rec| f(&path, rec))?;
    }
    Ok(())
}

/// Return the highest index among `*-######.bin` files.
pub fn highest_existing_index(dir: &Path) -> Result<u64> {
    let mut max_idx: u64 = 0;
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if !entry.file_type()?.is_file() {
            continue;
        }
        if let Some(idx) = parse_index_from_filename(&entry.path()) {
            if idx > max_idx {
                max_idx = idx;
            }
        }
    }
    Ok(max_idx)
}

fn parse_index_from_filename(path: &Path) -> Option<u64> {
    let name = path.file_name()?.to_str()?;
    if !name.ends_with(".bin") {
        return None;
    }
    let dash = name.rfind('-')?;
    let idx_part = &name[dash + 1..name.len() - 4]; // strip ".bin"
    if idx_part.len() == 6 && idx_part.chars().all(|c| c.is_ascii_digit()) {
        idx_part.parse::<u64>().ok()
    } else {
        None
    }
}
