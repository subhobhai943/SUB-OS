//! VFS Directory Entry Cache (dcache)
//! Memory-safe LRU path cache for high-speed file lookup

const DCACHE_CAPACITY: usize = 64;
const MAX_PATH_LEN: usize = 64;

#[derive(Copy, Clone)]
pub struct DCacheEntry {
    pub path: [u8; MAX_PATH_LEN],
    pub path_len: usize,
    pub inode: u64,
    pub is_dir: bool,
    pub valid: bool,
}

pub struct DCache {
    entries: [DCacheEntry; DCACHE_CAPACITY],
    hits: u64,
    misses: u64,
}

impl DCache {
    pub const fn new() -> Self {
        Self {
            entries: [DCacheEntry {
                path: [0; MAX_PATH_LEN],
                path_len: 0,
                inode: 0,
                is_dir: false,
                valid: false,
            }; DCACHE_CAPACITY],
            hits: 0,
            misses: 0,
        }
    }

    fn hash_path(path: &[u8]) -> usize {
        let mut hash: usize = 5381;
        for &b in path {
            hash = hash.wrapping_mul(33).wrapping_add(b as usize);
        }
        hash % DCACHE_CAPACITY
    }

    pub fn lookup(&mut self, path: &[u8]) -> Option<(u64, bool)> {
        let idx = Self::hash_path(path);
        let entry = &self.entries[idx];

        if entry.valid && entry.path_len == path.len() && &entry.path[..entry.path_len] == path {
            self.hits += 1;
            Some((entry.inode, entry.is_dir))
        } else {
            self.misses += 1;
            None
        }
    }

    pub fn insert(&mut self, path: &[u8], inode: u64, is_dir: bool) {
        if path.len() > MAX_PATH_LEN {
            return;
        }
        let idx = Self::hash_path(path);
        let entry = &mut self.entries[idx];
        entry.path[..path.len()].copy_from_slice(path);
        entry.path_len = path.len();
        entry.inode = inode;
        entry.is_dir = is_dir;
        entry.valid = true;
    }

    pub fn stats(&self) -> (u64, u64) {
        (self.hits, self.misses)
    }
}

static mut GLOBAL_DCACHE: DCache = DCache::new();

#[no_mangle]
pub extern "C" fn rust_dcache_lookup(path: *const u8, len: usize, out_inode: *mut u64, out_is_dir: *mut bool) -> i32 {
    if path.is_null() || out_inode.is_null() || out_is_dir.is_null() {
        return -1;
    }
    unsafe {
        let slice = core::slice::from_raw_parts(path, len);
        if let Some((inode, is_dir)) = GLOBAL_DCACHE.lookup(slice) {
            *out_inode = inode;
            *out_is_dir = is_dir;
            return 0; // Cache HIT
        }
    }
    -1 // Cache MISS
}

#[no_mangle]
pub extern "C" fn rust_dcache_insert(path: *const u8, len: usize, inode: u64, is_dir: bool) {
    if path.is_null() {
        return;
    }
    unsafe {
        let slice = core::slice::from_raw_parts(path, len);
        GLOBAL_DCACHE.insert(slice, inode, is_dir);
    }
}

#[no_mangle]
pub extern "C" fn rust_dcache_get_metrics(out_hits: *mut u64, out_misses: *mut u64) {
    if out_hits.is_null() || out_misses.is_null() {
        return;
    }
    unsafe {
        let (hits, misses) = GLOBAL_DCACHE.stats();
        *out_hits = hits;
        *out_misses = misses;
    }
}
