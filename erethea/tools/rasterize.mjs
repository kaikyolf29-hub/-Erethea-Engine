// Render SVG files to PNG with Chromium (Playwright).
// Usage: node rasterize.mjs <in.svg> <out.png> <width> <height> [<in.svg> <out.png> <width> <height> ...]

import { readFileSync } from "node:fs";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const args = process.argv.slice(2);
if (args.length === 0 || args.length % 4 !== 0) {
	console.error("Usage: node rasterize.mjs <in.svg> <out.png> <width> <height> ...");
	process.exit(1);
}

const browser = await chromium.launch();
try {
	for (let i = 0; i < args.length; i += 4) {
		const [src, dst, w, h] = args.slice(i, i + 4);
		const width = Number(w);
		const height = Number(h);
		const data = Buffer.from(readFileSync(src)).toString("base64");
		const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor: 1 });
		await page.setContent(
			`<html><body style="margin:0;background:transparent">` +
				`<img src="data:image/svg+xml;base64,${data}" style="width:${width}px;height:${height}px;display:block">` +
				`</body></html>`,
		);
		await page.screenshot({ path: dst, omitBackground: true });
		await page.close();
	}
} finally {
	await browser.close();
}
