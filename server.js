const express = require('express');
const multer = require('multer');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const { v4: uuidv4 } = require('uuid');
const AdmZip = require('adm-zip');

const GPP = process.platform === 'win32'
  ? 'C:\\Users\\Tim\\AppData\\Local\\Microsoft\\WinGet\\Packages\\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\\mingw64\\bin\\g++.exe'
  : 'x86_64-w64-mingw32-g++';

const app = express();
const PORT = process.env.PORT || 3000;

const UPLOAD_DIR = path.join(__dirname, 'uploads');
const OUTPUT_DIR = path.join(__dirname, 'outputs');

fs.mkdirSync(UPLOAD_DIR, { recursive: true });
fs.mkdirSync(OUTPUT_DIR, { recursive: true });

const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, UPLOAD_DIR),
  filename: (req, file, cb) => {
    const id = uuidv4();
    cb(null, `${id}.cpp`);
  }
});

const upload = multer({
  storage,
  limits: { fileSize: 5 * 1024 * 1024 },
  fileFilter: (req, file, cb) => {
    const ext = path.extname(file.originalname).toLowerCase();
    if (ext === '.cpp' || ext === '.zip') return cb(null, true);
    cb(new Error('Only .cpp and .zip files are allowed'));
  }
});

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json({ limit: '5mb' }));

app.post('/api/compile/upload', (req, res) => {
  upload.single('file')(req, res, (err) => {
    if (err) return res.status(400).json({ error: err.message });
    if (!req.file) return res.status(400).json({ error: 'No file uploaded' });

    const jobId = path.basename(req.file.filename, '.cpp');
    compileFile(req.file.path, jobId, res);
  });
});

app.post('/api/compile/paste', (req, res) => {
  const { code, filename } = req.body;
  if (!code || !code.trim()) {
    return res.status(400).json({ error: 'No code provided' });
  }

  const jobId = uuidv4();
  const cppPath = path.join(UPLOAD_DIR, `${jobId}.cpp`);
  const safeName = (filename || 'main.cpp').replace(/[^a-zA-Z0-9_\-\.]/g, '_');

  fs.writeFileSync(cppPath, code, 'utf-8');
  compileFile(cppPath, jobId, res);
});

app.post('/api/compile/project', (req, res) => {
  upload.single('file')(req, res, (err) => {
    if (err) return res.status(400).json({ error: err.message });
    if (!req.file) return res.status(400).json({ error: 'No file uploaded' });

    const ext = path.extname(req.file.originalname).toLowerCase();
    if (ext !== '.zip') {
      cleanup(req.file.path);
      return res.status(400).json({ error: 'Only .zip files are allowed' });
    }

    const jobId = uuidv4();
    const projectDir = path.join(UPLOAD_DIR, `project-${jobId}`);

    try {
      const zip = new AdmZip(req.file.path);
      zip.extractAllTo(projectDir, true);
      cleanup(req.file.path);

      const cppFiles = [];
      function collectCpp(dir) {
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const full = path.join(dir, entry.name);
          if (entry.isDirectory()) collectCpp(full);
          else if (entry.name.endsWith('.cpp')) cppFiles.push(full);
        }
      }
      collectCpp(projectDir);

      if (cppFiles.length === 0) {
        cleanup(projectDir);
        return res.json({ success: false, output: 'No .cpp files found in the ZIP' });
      }

      const exePath = path.join(OUTPUT_DIR, `${jobId}.exe`);
      const filesStr = cppFiles.map(f => `"${f}"`).join(' ');
      exec(`"${GPP}" ${filesStr} -o "${exePath}" -static`, { timeout: 60000 }, (error, stdout, stderr) => {
        cleanup(projectDir);
        if (error) {
          return res.json({ success: false, output: stderr || error.message });
        }
        res.json({
          success: true,
          downloadUrl: `/api/download/${jobId}`,
          jobId,
          files: cppFiles.length
        });
      });
    } catch (e) {
      cleanup(projectDir, req.file.path);
      res.status(500).json({ error: e.message });
    }
  });
});

function compileFile(cppPath, jobId, res) {
  const exePath = path.join(OUTPUT_DIR, `${jobId}.exe`);

  exec(`"${GPP}" "${cppPath}" -o "${exePath}" -static`, { timeout: 30000 }, (error, stdout, stderr) => {
    if (error) {
      cleanup(cppPath);
      return res.json({ success: false, output: stderr || error.message });
    }

    res.json({
      success: true,
      downloadUrl: `/api/download/${jobId}`,
      jobId
    });
  });
}

app.get('/api/download/:jobId', (req, res) => {
  const { jobId } = req.params;
  const exePath = path.join(OUTPUT_DIR, `${jobId}.exe`);

  if (!fs.existsSync(exePath)) {
    return res.status(404).json({ error: 'File not found or expired' });
  }

  res.download(exePath, 'program.exe', (err) => {
    if (!err) {
      cleanup(path.join(UPLOAD_DIR, `${jobId}.cpp`));
      fs.unlink(exePath, () => {});
    }
  });
});

app.get('/api/status/:jobId', (req, res) => {
  const exePath = path.join(OUTPUT_DIR, `${req.params.jobId}.exe`);
  res.json({ ready: fs.existsSync(exePath) });
});

function cleanup(...files) {
  files.forEach(f => {
    if (f && fs.existsSync(f)) {
      fs.unlink(f, () => {});
    }
  });
}

setInterval(() => {
  const maxAge = 10 * 60 * 1000;
  const now = Date.now();

  [UPLOAD_DIR, OUTPUT_DIR].forEach(dir => {
    fs.readdir(dir, (err, files) => {
      if (err) return;
      files.forEach(file => {
        const fp = path.join(dir, file);
        fs.stat(fp, (err, stat) => {
          if (err) return;
          if (now - stat.mtimeMs > maxAge) fs.unlink(fp, () => {});
        });
      });
    });
  });
}, 5 * 60 * 1000);

app.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}`);
});
