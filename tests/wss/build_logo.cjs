const fs=require('fs');const sharp=require('sharp');
// Run from the repository root with sharp installed (or available via NODE_PATH).
(async()=>{
let mark=await sharp('src/graphics/assets/movetain-mark.svg').resize(42,24,{fit:'contain'}).png().toBuffer();
let word=await sharp('src/graphics/assets/movetain-wordmark.svg').resize(60,7,{fit:'contain'}).png().toBuffer();
let {data}=await sharp({create:{width:64,height:32,channels:4,background:'#000'}}).composite([{input:mark,left:11,top:0},{input:word,left:2,top:25}]).removeAlpha().greyscale().raw().toBuffer({resolveWithObject:true});
const bytes=[];for(let y=0;y<32;y++)for(let b=0;b<8;b++){let v=0;for(let k=0;k<8;k++)if(data[y*64+b*8+k]>=96)v|=1<<k;bytes.push(v);}
fs.mkdirSync('src/graphics/assets',{recursive:true});fs.writeFileSync('src/graphics/assets/MovetainLogo.h','#pragma once\n#include <stdint.h>\n// Monochrome 64x32 raster of the portal logom.svg and textlogo.svg.\nnamespace mvtScreen {\nstatic const uint8_t logoBits[256] = {\n'+Array.from({length:16},(_,i)=>'    '+bytes.slice(i*16,i*16+16).map(x=>'0x'+x.toString(16).padStart(2,'0')).join(',')).join(',\n')+'\n};\ntemplate<class Display> void logo(Display *display, int x, int y) {\n    for (int py=0;py<32;++py) for (int px=0;px<64;++px)\n        if (logoBits[py*8+px/8] & (1<<(px%8))) display->setPixel(x+px,y+py);\n}\n}\n');
for(let i=0;i<data.length;i++)data[i]=data[i]>=96?255:0;
fs.mkdirSync('.tools',{recursive:true});
await sharp(data,{raw:{width:64,height:32,channels:1}}).resize(640,320,{kernel:'nearest'}).png().toFile('.tools/movetain-splash.png');
})();
