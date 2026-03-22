# GitHub Pages Setup Guide

Your WASM build is now configured to automatically deploy to GitHub Pages! Follow these steps to complete the setup.

## Quick Setup (3 steps)

### 1. Enable GitHub Pages in Repository Settings

1. Go to your repository on GitHub
2. Click **Settings** (top navigation)
3. Click **Pages** (left sidebar)
4. Under **Source**, select **GitHub Actions**
5. Click **Save**

That's it! No need to select a branch - GitHub Actions will handle deployment.

### 2. Set Required Permissions

The workflow already includes the necessary permissions:
```yaml
permissions:
  pages: write
  id-token: write
```

But you may need to enable workflow permissions:

1. Go to **Settings** → **Actions** → **General**
2. Scroll to **Workflow permissions**
3. Select **Read and write permissions**
4. Check **Allow GitHub Actions to create and approve pull requests**
5. Click **Save**

### 3. Push Your Code

```bash
git add .github/workflows/build.yml
git commit -m "Enable GitHub Pages deployment for WASM build"
git push origin master  # or 'main' depending on your default branch
```

## What Happens Next

1. **GitHub Actions runs** the build workflow
2. **WASM is compiled** using Emscripten
3. **Build artifacts** (*.wasm, *.js, *.html) are uploaded
4. **GitHub Pages deploys** the WASM build automatically
5. **Your site goes live** at: `https://YOUR_USERNAME.github.io/YOUR_REPO/`

## Viewing Your Deployment

After the workflow completes:

1. Go to **Actions** tab in your repository
2. Click on the latest workflow run
3. Look for the **deploy-pages** job
4. The deployment URL will be shown in the job output
5. Or go to **Settings** → **Pages** to see your live URL

## Testing Locally Before Deployment

To test the WASM build locally:

```bash
cd wasm
emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Serve locally (requires Python 3)
cd build
python3 -m http.server 8000

# Open http://localhost:8000 in your browser
```

## Files Included in Deployment

The following files are deployed to GitHub Pages:

- `*.wasm` - WebAssembly binaries
- `*.js` - JavaScript glue code
- `index.html` - Main HTML page
- Any files from `wasm/dist/` directory
- `.nojekyll` - Prevents Jekyll processing (important!)

## Deployment Triggers

The WASM site is automatically deployed when:

- You push to `master` or `main` branch
- The `build-wasm` job completes successfully
- The workflow has write permissions

**Note:** Pull requests will build but NOT deploy to Pages.

## Custom Domain (Optional)

To use a custom domain:

1. Add a `CNAME` file to `wasm/build/` with your domain name
2. Update the workflow to copy it during build
3. Configure DNS at your domain registrar
4. Go to **Settings** → **Pages** → **Custom domain**

## Troubleshooting

### "Deploy to GitHub Pages" job fails with permissions error

**Fix:** Go to Settings → Actions → General → Workflow permissions → Select "Read and write permissions"

### WASM files load but nothing appears

**Check:**
- Browser console for errors
- MIME types are correct (GitHub Pages should handle this automatically)
- CORS issues (shouldn't happen with GitHub Pages)
- Try accessing via HTTPS not HTTP

### Changes don't appear on the site

**Solutions:**
- Hard refresh: Ctrl+F5 (Windows/Linux) or Cmd+Shift+R (Mac)
- Clear browser cache
- Check if workflow actually deployed (green checkmark in Actions tab)
- Wait a few minutes for CDN propagation

### Site shows 404

**Check:**
- GitHub Pages is enabled (Settings → Pages)
- Source is set to "GitHub Actions"
- The workflow completed successfully
- You're using the correct URL format

### index.html missing

**Fix:** Make sure `wasm/index.html` exists. If not, create one:

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Hyades WASM Demo</title>
</head>
<body>
    <h1>Hyades WASM</h1>
    <script type="module">
        import Module from './hyades.js';

        Module().then(m => {
            console.log('WASM loaded!', m);
            // Your code here
        });
    </script>
</body>
</html>
```

## Monitoring Deployments

### View deployment history
- Go to **Settings** → **Pages**
- Click **View deployments** button
- See all historical deployments

### Check build logs
- Go to **Actions** tab
- Click on any workflow run
- Expand jobs to see detailed logs

### Deployment status badge

Add to your README.md:

```markdown
[![Deploy Pages](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml)
```

## Security Notes

- GitHub Pages sites are **always public**, even for private repositories
- Don't include secrets or sensitive data in your WASM build
- The `.nojekyll` file prevents GitHub from processing certain files
- CORS is automatically configured correctly by GitHub Pages

## Next Steps

Once deployed, you can:

1. Share the URL with others to try your WASM build
2. Embed the WASM module in other web pages
3. Set up a custom domain
4. Add analytics to track usage
5. Create a nice landing page with examples

## Need Help?

- [GitHub Pages Documentation](https://docs.github.com/en/pages)
- [GitHub Actions for Pages](https://github.com/actions/deploy-pages)
- Check the Actions tab for build logs
- Open an issue in your repository

---

**Your Pages URL:** `https://YOUR_USERNAME.github.io/YOUR_REPO/`

Replace `YOUR_USERNAME` and `YOUR_REPO` with your actual GitHub username and repository name.
