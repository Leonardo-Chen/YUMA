# YUMA · 反思式骑乘系统

让看不见的变得可见。

YUMA 是一套可穿戴传感系统，通过智能马镫、可穿戴节点、实时可视化与 AI 辅助反思，将骑手与马匹的互动转化为可量化的洞察。

**官方网站：** [www.yuma.li](https://www.yuma.li)  
**GitHub Pages：** [leonardo-chen.github.io/YUMA](https://leonardo-chen.github.io/YUMA/)

[English](./README.md) | 中文

## 关于项目

本仓库托管 YUMA 官方产品网站：一个基于原生 HTML、CSS 与 JavaScript 的单页响应式落地页，无需构建工具或前端框架。

网站面向骑手、教练与马术团队，介绍 YUMA 产品生态：

- **智能马镫（Smart Stirrups）** — 压力、平衡与下肢节奏传感
- **可穿戴节点（Wearable Nodes）** — 骑手姿态与马匹运动追踪
- **移动应用（Mobile App）** — 训练准备、设备状态与骑乘回顾
- **实时仪表盘（Live Dashboard）** — 实时可视化与 AI 反思

## 功能特性

- 适配桌面、iPad 与 iPhone 的响应式布局
- 浅色 / 深色主题切换
- 多语言界面：English、中文、Deutsch、Français、Italiano
- Hero 背景视频与 YouTube 演示播放
- 通过 USDZ 模型提供 AR 产品预览（Safari / iOS）
- 通过 GitHub Pages 的 `CNAME` 支持自定义域名

## 项目结构

```text
YUMA/
├── index.html          # 完整站点（结构、样式、脚本）
├── video.mp4           # Hero 背景视频
├── CNAME               # GitHub Pages 自定义域名
├── img/                # 产品图、Logo、Open Graph 分享图
└── models/             # AR 预览用 USDZ 模型
    ├── stirrup.usdz
    └── rider.usdz
```

## 本地预览

站点为纯静态页面，任意本地 HTTP 服务均可预览。直接用 `file://` 打开 `index.html` 可能限制 YouTube 嵌入与部分浏览器功能。

```bash
# Python
python3 -m http.server 8000

# Node
npx serve .
```

然后在浏览器打开 [http://localhost:8000](http://localhost:8000)。

## 部署

本站通过 **GitHub Pages** 从本仓库发布。自定义域名配置在 `CNAME` 中，为 `www.yuma.li`。

将更改推送到默认分支后，GitHub Pages 会自动更新线上站点。

## 联系

如需演示、试点合作或商务联系，请通过官网联系区：[www.yuma.li](https://www.yuma.li/#contact)。

---

© 2026 YUMA · 智能马术科技
