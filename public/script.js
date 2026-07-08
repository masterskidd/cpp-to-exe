let selectedFile = null;
let selectedZip = null;
let isCompiling = false;

const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const fileBtn = document.getElementById('fileBtn');
const fileName = document.getElementById('fileName');

const projectDropZone = document.getElementById('projectDropZone');
const projectInput = document.getElementById('projectInput');
const projectBtn = document.getElementById('projectBtn');
const projectFileName = document.getElementById('projectFileName');

const fileTab = document.querySelector('[data-tab="upload"]');
const projectTab = document.querySelector('[data-tab="project"]');
const pasteTab = document.querySelector('[data-tab="paste"]');
const fileTabContent = document.getElementById('tab-upload');
const projectTabContent = document.getElementById('tab-project');
const pasteTabContent = document.getElementById('tab-paste');
const codeArea = document.getElementById('codeArea');
const filenameInput = document.getElementById('filenameInput');
const convertBtn = document.getElementById('convertBtn');
const status = document.getElementById('status');
const statusText = document.getElementById('statusText');
const spinner = document.getElementById('spinner');
const progressFill = document.getElementById('progressFill');
const result = document.getElementById('result');
const downloadBtn = document.getElementById('downloadBtn');

let lastJobId = null;

fileBtn.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (file) handleFileSelect(file);
});
dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => { dropZone.classList.remove('dragover'); });
dropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropZone.classList.remove('dragover');
  const file = e.dataTransfer.files[0];
  if (file && file.name.endsWith('.cpp')) handleFileSelect(file);
  else { fileName.textContent = 'Please drop a .cpp file'; fileName.style.color = '#ff6b6b'; }
});

projectBtn.addEventListener('click', () => projectInput.click());
projectInput.addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (file) handleZipSelect(file);
});
projectDropZone.addEventListener('click', () => projectInput.click());
projectDropZone.addEventListener('dragover', (e) => { e.preventDefault(); projectDropZone.classList.add('dragover'); });
projectDropZone.addEventListener('dragleave', () => { projectDropZone.classList.remove('dragover'); });
projectDropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  projectDropZone.classList.remove('dragover');
  const file = e.dataTransfer.files[0];
  if (file && file.name.endsWith('.zip')) handleZipSelect(file);
  else { projectFileName.textContent = 'Please drop a .zip file'; projectFileName.style.color = '#ff6b6b'; }
});

codeArea.addEventListener('input', updateConvertBtn);
filenameInput.addEventListener('input', updateConvertBtn);

function handleFileSelect(file) {
  selectedFile = file;
  fileName.textContent = file.name;
  fileName.style.color = '#6c5ce7';
  fileInput.value = '';
  updateConvertBtn();
}

function handleZipSelect(file) {
  selectedZip = file;
  projectFileName.textContent = file.name;
  projectFileName.style.color = '#6c5ce7';
  projectInput.value = '';
  updateConvertBtn();
}

function getActiveTab() {
  if (fileTabContent.classList.contains('active')) return 'upload';
  if (projectTabContent.classList.contains('active')) return 'project';
  return 'paste';
}

function updateConvertBtn() {
  const tab = getActiveTab();
  if (tab === 'upload') convertBtn.disabled = !selectedFile;
  else if (tab === 'project') convertBtn.disabled = !selectedZip;
  else convertBtn.disabled = !codeArea.value.trim();
}

fileTab.addEventListener('click', () => switchTab('upload'));
projectTab.addEventListener('click', () => switchTab('project'));
pasteTab.addEventListener('click', () => switchTab('paste'));

function switchTab(tab) {
  fileTab.classList.toggle('active', tab === 'upload');
  projectTab.classList.toggle('active', tab === 'project');
  pasteTab.classList.toggle('active', tab === 'paste');
  fileTabContent.classList.toggle('active', tab === 'upload');
  projectTabContent.classList.toggle('active', tab === 'project');
  pasteTabContent.classList.toggle('active', tab === 'paste');
  updateConvertBtn();
  hideAll();
}

convertBtn.addEventListener('click', async () => {
  if (isCompiling) return;
  hideAll();
  isCompiling = true;
  convertBtn.disabled = true;
  showStatus('Compiling...', true);
  setProgress(30);

  try {
    const tab = getActiveTab();
    let response;

    if (tab === 'upload' && selectedFile) {
      const fd = new FormData();
      fd.append('file', selectedFile);
      setProgress(50);
      response = await fetch('/api/compile/upload', { method: 'POST', body: fd });
    } else if (tab === 'project' && selectedZip) {
      const fd = new FormData();
      fd.append('file', selectedZip);
      setProgress(50);
      response = await fetch('/api/compile/project', { method: 'POST', body: fd });
    } else {
      const code = codeArea.value;
      const fn = filenameInput.value.trim() || 'main.cpp';
      setProgress(50);
      response = await fetch('/api/compile/paste', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ code, filename: fn })
      });
    }

    setProgress(80);
    const data = await response.json();

    if (data.success) {
      setProgress(100);
      setTimeout(() => {
        hideStatus();
        showResult(data.downloadUrl, data.jobId);
        isCompiling = false;
      }, 400);
    } else {
      setProgress(100);
      setTimeout(() => {
        hideStatus();
        showError(data.output);
        isCompiling = false;
        convertBtn.disabled = false;
      }, 400);
    }
  } catch (err) {
    hideStatus();
    showError('Connection error: ' + err.message);
    isCompiling = false;
    convertBtn.disabled = false;
  }
});

function showStatus(text, showSpinner) {
  status.classList.add('visible');
  statusText.textContent = text;
  spinner.classList.toggle('visible', showSpinner);
}

function hideStatus() {
  status.classList.remove('visible');
  spinner.classList.remove('visible');
  setProgress(0);
}

function setProgress(pct) {
  progressFill.style.width = pct + '%';
}

function showResult(url, jobId) {
  lastJobId = jobId;
  result.classList.add('visible');
  downloadBtn.href = url;
}

function hideResult() {
  result.classList.remove('visible');
}

function showError(msg) {
  const existing = document.querySelector('.error-box');
  if (existing) existing.remove();
  const box = document.createElement('div');
  box.className = 'error-box visible';
  box.innerHTML = '<pre>' + escapeHtml(msg) + '</pre>';
  document.querySelector('.container').appendChild(box);
}

function hideAll() {
  hideResult();
  hideStatus();
  const err = document.querySelector('.error-box');
  if (err) err.remove();
}

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

updateConvertBtn();
