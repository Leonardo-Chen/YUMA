# YUMA · Reflective Riding System

Making the invisible visible.

YUMA is a wearable sensing system that turns rider–horse interaction into measurable insights through smart stirrups, wearable nodes, live visualization, and AI-assisted reflection.

**Live site:** [www.yuma.li](https://www.yuma.li)  
**GitHub Pages:** [leonardo-chen.github.io/YUMA](https://leonardo-chen.github.io/YUMA/)

English | [中文](./README.zh-CN.md)

## About

This repository hosts the official YUMA product website — a single-page, responsive landing experience built with plain HTML, CSS, and JavaScript. No build step or framework is required.

The site introduces the YUMA ecosystem for riders, coaches, and equestrian teams:

- **Smart Stirrups** — pressure, balance, and lower-body rhythm sensing
- **Wearable Nodes** — rider posture and horse motion tracking
- **Mobile App** — session preparation, device status, and ride review
- **Live Dashboard** — real-time visualization and AI reflection

## Features

- Responsive layout for desktop, iPad, and iPhone
- Light / dark theme toggle
- Multilingual UI: English, 中文, Deutsch, Français, Italiano
- Hero background video and YouTube demo playback
- AR product previews via USDZ models (Safari / iOS)
- Custom domain support through GitHub Pages (`CNAME`)

## Project structure

```text
YUMA/
├── index.html          # Full site (markup, styles, scripts)
├── video.mp4           # Hero background video
├── CNAME               # Custom domain for GitHub Pages
├── img/                # Product images, logo, Open Graph assets
└── models/             # USDZ models for AR previews
    ├── stirrup.usdz
    └── rider.usdz
```

## Local preview

Because the site is static, any local HTTP server works. Opening `index.html` directly via `file://` may limit YouTube embeds and some browser features.

```bash
# Python
python3 -m http.server 8000

# Node
npx serve .
```

Then open [http://localhost:8000](http://localhost:8000).

## Deploy

The site is published with **GitHub Pages** from this repository. The custom domain is configured in `CNAME` as `www.yuma.li`.

After pushing changes to the default branch, GitHub Pages will update the live site.

## Contact

Interested in demos, pilots, or partnerships? Reach out via the contact section on [www.yuma.li](https://www.yuma.li/#contact).

---

© 2026 YUMA · Intelligent equestrian technology
