#!/usr/bin/env node
// Hand-authored, code-native fan-art sprites. Not extracted official artwork.
// Character rights are not granted by this repository's code license.
import {writeFileSync, readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {dirname, resolve} from 'node:path';
import {createHash} from 'node:crypto';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const palette = [0,0x202335,0x424557,0xFFF9E6,0xFFE6A4,0xFFCE32,0xF59722,0x976247,
  0x405A9C,0x5CC3CD,0xFFADC0,0xDD668B,0xCCD5E0,0x7F90AA,0x9668B2,0xD94D3D];
const names = [
  ['egg','botamon','koromon','agumon','greymon','metalgreymon','wargreymon'],
  ['egg','punimon','tsunomon','gabumon','garurumon','weregarurumon','metalgarurumon'],
  ['egg','poyomon','tokomon','patamon','angemon','magnaangemon','seraphimon'],
];
const W=32,H=28;
let p;
function dot(x,y,c) { if(x>=0&&y>=0&&x<W&&y<H) p[y*W+x]=c; }
function rect(x,y,w,h,c) { for(let j=y;j<y+h;j++) for(let i=x;i<x+w;i++) dot(i,j,c); }
function poly(points,c) {
  for(let y=0;y<H;y++) for(let x=0;x<W;x++) {
    let hit=false;
    for(let i=0,j=points.length-1;i<points.length;j=i++) {
      const [a,b]=points[i],[d,e]=points[j];
      if((b>y+.5)!=(e>y+.5) && x+.5<(d-a)*(y+.5-b)/(e-b)+a) hit=!hit;
    }
    if(hit) dot(x,y,c);
  }
}
function eye(x,y,sleep) {
  if(sleep) {rect(x,y+1,3,1,1);return;}
  rect(x,y,3,3,3);rect(x+1,y,1,2,1);
}
function dinosaur(stage,sleep,eat) {
  const adult=stage>=4, metal=stage===5;
  if(metal) {
    poly([[17,10],[25,3],[24,10],[30,7],[26,15],[20,16]],14);
    poly([[18,11],[24,6],[22,13]],11);
    poly([[12,10],[8,4],[7,12]],14);
  }
  // Heavy tail, thighs and large three-clawed feet distinguish the dinosaur.
  poly([[19,16],[23,19],[28,18],[30,15],[29,22],[22,24],[17,21]],6);
  poly([[11,13],[20,12],[23,18],[21,24],[10,24],[7,20]],6);
  poly([[12,16],[18,15],[20,21],[16,24],[11,21]],4);
  rect(9,22,5,3,5); rect(17,22,5,3,6);
  for(const x of [8,10,12,17,19,21]) rect(x,25,1,1,3);
  poly([[9,13],[6,15],[5,19],[8,18],[10,16]],5);
  rect(4,18,1,2,3); rect(6,19,1,1,3);
  poly([[19,14],[24,15],[25,18],[22,19],[20,17]],6);
  for(const x of [22,24]) rect(x,19,1,1,3);
  // Broad snout and a prominent visible eye, facing left.
  poly([[10,5],[17,4],[22,8],[21,13],[15,16],[8,14],[5,11],[6,8],[10,8]],5);
  poly([[16,5],[21,8],[20,13],[16,15],[14,13],[18,11]],6);
  rect(5,10,2,2,5); dot(6,10,1);
  rect(7,13,7,eat?3:1,1); if(eat) rect(8,15,5,1,15);
  dot(8,13,3);dot(11,13,3);
  if(adult) {
    // Greymon's brown horned helmet and blue tiger stripes.
    poly([[8,8],[10,4],[17,3],[22,7],[21,11],[18,12],[17,8],[12,9]],metal?12:7);
    poly([[10,5],[9,1],[13,4]],metal?12:4);
    poly([[17,4],[19,1],[20,6]],metal?12:4);
    poly([[8,8],[4,5],[5,10]],metal?12:4);
    poly([[10,17],[12,16],[14,19],[12,19]],8);
    poly([[18,19],[22,18],[22,20],[19,21]],8);
    rect(23,21,2,1,8); rect(10,23,2,1,8);
    eye(11,9,sleep);
  } else eye(10,8,sleep);
  if(metal) {
    rect(18,7,3,4,13); dot(19,8,15);
    // Metal left arm / trident and two chest missile ports.
    poly([[20,14],[24,14],[26,18],[25,22],[22,22],[21,18]],13);
    rect(21,15,3,3,12);rect(22,18,3,2,12);
    for(const x of [22,24,26]) rect(x,21,1,3,12);
    rect(11,16,7,4,7);rect(12,17,2,2,1);rect(15,17,2,2,1);
  }
}
function wolf(stage,sleep,eat) {
  if(stage===1) {
    // Punimon: red jelly body, three soft antennae.
    for(const [x,y] of [[9,9],[15,6],[21,9]]) rect(x,y,3,8,15);
    poly([[9,15],[23,15],[26,20],[24,24],[8,24],[6,20]],15);
    eye(10,18,sleep);eye(19,18,sleep);
    rect(14,22,4,eat?2:1,1);
  } else if(stage===2) {
    // Tsunomon: a single horn over the orange fur and pale round face.
    poly([[14,12],[16,3],[18,12]],12);
    poly([[10,12],[21,12],[25,17],[24,23],[20,25],[10,25],[6,20],[7,16]],7);
    poly([[10,16],[21,16],[23,20],[20,24],[10,24],[8,21]],4);
    eye(10,18,sleep);eye(18,18,sleep);rect(14,22,3,eat?2:1,1);
  } else if(stage===3) {
    // Gabumon's blue-striped fur hood around a yellow face and belly.
    poly([[9,7],[22,7],[25,12],[24,22],[20,25],[10,25],[6,20],[7,12]],3);
    poly([[15,8],[16,2],[19,7]],12);
    poly([[11,11],[19,11],[21,15],[19,18],[20,23],[12,24],[10,18],[8,16]],5);
    rect(12,20,7,2,6);eye(11,12,sleep);eye(17,12,sleep);
    rect(12,16,6,eat?2:1,1);
    for(const [x,y] of [[8,9],[20,10],[7,18],[21,18],[8,22],[21,23]]) rect(x,y,3,2,8);
    rect(8,24,4,2,5);rect(20,24,4,2,5);
  } else if(stage===5) {
    // WereGarurumon stands upright: fur ears, bare chest, blue ripped trousers.
    poly([[11,8],[10,2],[15,6],[20,3],[21,10],[18,13],[11,12]],3);
    rect(11,8,8,2,8);eye(12,9,sleep);rect(10,12,7,eat?2:1,1);
    poly([[12,13],[19,13],[22,19],[18,21],[11,20],[9,17]],3);
    poly([[10,19],[21,19],[22,25],[18,25],[16,22],[14,25],[9,25]],8);
    rect(10,22,3,1,3);rect(18,23,3,1,3);
    poly([[10,13],[7,14],[4,19],[7,20],[12,16]],3);
    poly([[21,13],[25,15],[28,19],[25,21],[20,17]],3);
    for(const x of [4,6,25,27]) rect(x,20,1,2,12);
    rect(10,14,3,2,8);rect(19,16,3,2,8);
  } else {
    const metal=stage===6;
    // Four-legged wolf; MetalGarurumon adds wing panels and missile pods.
    if(metal) {
      poly([[16,13],[19,3],[23,5],[22,12],[29,9],[26,16]],8);
      poly([[19,10],[21,5],[22,6],[21,11]],12);
    }
    poly([[23,16],[28,12],[30,8],[30,18],[25,22],[20,20]],metal?13:3);
    poly([[10,13],[24,13],[26,19],[22,22],[10,21],[7,17]],metal?12:3);
    poly([[8,9],[8,3],[12,8],[16,5],[17,13],[12,17],[5,15],[3,12]],metal?13:3);
    eye(8,10,sleep);rect(3,14,8,eat?2:1,1);dot(4,12,1);
    for(const x of [9,13,20,24]) {rect(x,20,2,5,metal?13:3);rect(x-1,24,3,1,12);}
    for(const [x,y] of [[11,8],[13,15],[19,14],[23,18],[26,17]]) rect(x,y,2,3,8);
    if(metal) {rect(16,17,8,3,8);dot(18,18,15);dot(21,18,15);rect(6,7,6,2,12);}
  }
}
function angel(stage,sleep,eat) {
  if(stage===1) {
    // Poyomon: translucent-looking blue jelly with tiny waving feet.
    poly([[12,12],[19,12],[23,16],[24,22],[21,24],[18,22],[16,25],[13,22],[10,24],[8,21],[9,16]],9);
    poly([[13,13],[19,13],[21,16],[11,16]],3);
    eye(11,17,sleep);eye(18,17,sleep);rect(14,21,4,eat?2:1,1);
  } else if(stage===2) {
    // Tokomon: tiny pink quadruped, short ears and an unexpectedly toothy mouth.
    rect(10,10,2,6,10);rect(20,10,2,6,10);
    poly([[10,15],[22,15],[25,19],[23,23],[9,23],[6,20]],3);
    for(const x of [8,12,19,23]) rect(x,22,2,3,10);
    eye(10,16,sleep);eye(18,16,sleep);rect(12,20,7,eat?3:1,1);
    for(const x of [12,14,16,18]) dot(x,20,3);
  } else if(stage===3) {
    // Patamon's huge orange ear-wings are the silhouette, not a recolored wolf.
    poly([[12,14],[8,6],[3,7],[5,14],[11,18]],6);
    poly([[20,14],[25,6],[29,7],[27,15],[21,18]],6);
    poly([[7,9],[10,14],[7,13]],4);poly([[25,9],[22,15],[26,12]],4);
    poly([[11,13],[21,13],[25,18],[23,23],[9,23],[6,19]],6);
    poly([[8,19],[24,19],[22,24],[10,24]],3);
    eye(11,15,sleep);eye(18,15,sleep);rect(13,20,5,eat?2:1,1);
    for(const x of [8,12,19,23]) rect(x,23,2,2,4);
  } else {
    const ultimate=stage===6, magna=stage===5;
    // Layered feather fans: six/eight/ten wings suggested at this resolution.
    const pairs=stage-1;
    for(let i=pairs-1;i>=0;i--) {
      const y=5+i*3;
      poly([[13,y+5],[8,y+2],[3,y-1],[3,y+3],[7,y+7],[13,y+8]],i%2?12:3);
      poly([[19,y+5],[24,y+2],[29,y-1],[29,y+3],[25,y+7],[19,y+8]],i%2?12:3);
    }
    poly([[12,12],[19,12],[21,19],[18,22],[12,21],[10,17]],ultimate?8:3);
    rect(13,14,6,3,ultimate?12:magna?14:5);
    rect(12,20,3,6,ultimate?13:3);rect(17,20,3,6,ultimate?13:3);
    rect(12,24,3,2,ultimate?8:7);rect(17,24,3,2,ultimate?8:7);
    poly([[12,7],[19,7],[20,11],[17,14],[13,13],[11,10]],ultimate?12:4);
    rect(11,7,9,3,ultimate?8:magna?14:12);
    rect(14,6,3,4,5);rect(13,10,2,1,sleep?1:9);rect(17,10,2,1,sleep?1:9);
    if(eat) rect(15,12,2,1,1);
    poly([[10,13],[7,16],[8,20],[11,17]],ultimate?12:3);
    poly([[20,13],[24,16],[23,20],[20,17]],ultimate?12:3);
    if(magna) {poly([[24,16],[28,10],[28,19],[24,22]],9);rect(22,19,4,2,14);}
    else if(!ultimate) {rect(6,10,1,16,5);rect(4,9,5,2,5);}
    else {rect(12,18,8,2,5);rect(14,15,4,2,9);}
  }
}
function sprite(line,stage,pose,dark=false) {
  p=new Uint8Array(W*H);
  const sleep=pose===2,eat=pose===1;
  if(dark && stage===5) {
    // SkullGreymon: hollow skull, visible ribs, spine missile and bone tail.
    poly([[11,5],[19,5],[22,9],[18,13],[8,12],[5,9],[6,6]],12);
    rect(8,7,4,3,1);rect(16,7,3,3,1);
    if(!sleep) {dot(10,8,15);dot(17,8,15);}
    rect(7,12,11,2,12);for(const x of [8,11,14,17]) rect(x,11,1,eat?4:2,3);
    rect(15,14,2,9,12);
    for(const y of [14,17,20]) {rect(10,y,12,1,3);rect(9,y+1,2,1,12);rect(21,y+1,2,1,12);}
    poly([[18,5],[21,2],[24,3],[21,7]],15);rect(19,4,4,2,12);
    poly([[22,19],[28,17],[30,12],[30,20],[26,22],[22,22]],12);
    rect(10,22,2,3,12);rect(20,22,2,3,12);
    rect(8,25,5,1,3);rect(20,25,5,1,3);
    rect(6,15,2,6,12);rect(23,14,2,5,12);
    for(const x of [4,6,8]) rect(x,20,1,3,3);
  } else if(stage===0) {
    poly([[14,5],[18,5],[22,11],[24,19],[22,24],[10,24],[8,19],[10,11]],3);
    poly([[20,10],[23,18],[21,23],[18,23],[20,18]],12);
    poly([[11,10],[15,11],[14,15],[10,14]],line===1?8:line===2?10:6);
    poly([[16,18],[20,18],[20,21],[17,23],[15,21]],line===1?8:line===2?10:6);
  } else if(line===1) wolf(stage,sleep,eat);
  else if(line===2) angel(stage,sleep,eat);
  else if(stage===1) {
    poly([[8,14],[8,10],[12,13],[17,12],[22,10],[22,14],[25,18],[24,23],[21,25],[10,25],[7,22],[6,18]],1);
    poly([[11,14],[17,13],[22,16],[21,22],[12,23],[8,20]],2);
    eye(11,17,sleep);eye(18,17,sleep);
    if(eat) {rect(14,21,4,2,1);rect(15,22,2,1,11);}
  } else if(stage===2) {
    poly([[11,15],[9,11],[8,3],[10,2],[13,10],[14,14]],10);
    poly([[18,14],[21,6],[24,3],[25,5],[23,12],[21,16]],10);
    poly([[11,14],[18,13],[23,15],[26,19],[25,23],[21,25],[10,25],[6,22],[6,18]],10);
    poly([[22,16],[25,19],[24,23],[19,25],[10,24],[19,23]],11);
    eye(10,18,sleep);eye(18,18,sleep);
    rect(13,22,5,eat?2:1,1);dot(14,22,3);dot(16,22,3);
  } else if(stage<6) dinosaur(stage,sleep,eat);
  else {
    // WarGreymon: split Brave Shield, red mane, silver helmet, gold armor,
    // and both oversized Dramon Killer claws. No dinosaur-body recolor.
    poly([[4,7],[12,9],[14,18],[5,17],[2,13]],5);
    poly([[20,9],[28,7],[30,13],[27,17],[18,18]],5);
    poly([[5,8],[11,10],[12,15],[5,14]],6);
    poly([[22,10],[27,8],[27,14],[21,15]],6);
    poly([[12,9],[10,5],[13,6],[14,2],[16,5],[18,2],[19,6],[22,5],[20,10]],15);
    poly([[12,10],[20,10],[23,15],[21,20],[11,20],[9,15]],5);
    rect(12,14,8,3,6);rect(13,17,6,3,2);
    poly([[11,20],[15,20],[14,25],[10,25]],6);
    poly([[17,20],[21,20],[22,25],[18,25]],6);
    rect(9,24,5,2,12);rect(18,24,5,2,12);
    poly([[7,13],[10,13],[12,18],[9,21],[5,19]],13);
    poly([[22,13],[25,13],[27,19],[23,21],[20,18]],13);
    rect(6,15,3,4,12);rect(23,15,3,4,12);
    for(const x of [5,7,9,22,24,26]) poly([[x,19],[x+1,19],[x+1,23],[x-1,24]],12);
    poly([[13,5],[19,5],[21,9],[19,12],[13,12],[11,9]],12);
    poly([[12,6],[9,4],[11,10],[13,9]],12);
    poly([[19,6],[23,4],[21,10],[19,9]],12);
    rect(13,8,6,2,1); dot(13,8,sleep?1:9);dot(18,8,sleep?1:9);
    rect(15,10,2,2,13);
    if(eat) rect(15,11,2,1,1);
  }
  if(dark && stage===6) {
    // Same WarGreymon silhouette, obsidian armor with gold mane/highlights.
    p=p.map(c=>c===5?2:c===6?8:c===15?5:c);
  }
  // One-pixel outside silhouette, deterministic and independent of LVGL.
  const original=p.slice();
  for(let y=1;y<H-1;y++) for(let x=1;x<W-1;x++) if(!original[y*W+x] &&
    [original[y*W+x-1],original[y*W+x+1],original[(y-1)*W+x],original[(y+1)*W+x]].some(Boolean)) dot(x,y,1);
  return p;
}
const sourceHash=createHash('sha256').update(readFileSync(fileURLToPath(import.meta.url))).digest('hex');
const output=['/* Generated by tools/generate_digimon_sprites.mjs. Character rights reserved. */',
  `/* Generator SHA256: ${sourceHash} */`,'#include "digimon_sprites.h"'];
function emitPixels(line,stage,pose,dark=false) {
  const pixels=sprite(line,stage,pose,dark), bytes=[];
  // RGB565 color plane followed by A8 avoids an indexed decoder conversion
  // before the exact 3x transform, and requires no mutable canvas per pet.
  for(const index of pixels) {
    const c=palette[index], rgb=((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31);
    bytes.push(rgb&255,rgb>>8);
  }
  for(const index of pixels) bytes.push(index?255:0);
  const id=dark?`dark_${stage}_${pose}`:`line${line}_${names[line][stage]}_${pose}`;
  output.push(`static const LV_ATTRIBUTE_MEM_ALIGN uint8_t ${id}[] = {`);
  for(let i=0;i<bytes.length;i+=16) output.push('    '+bytes.slice(i,i+16).map(x=>'0x'+x.toString(16).padStart(2,'0')).join(', ')+',');
  output.push('};');
}
for(let line=0;line<3;line++) for(let stage=0;stage<7;stage++) for(let pose=0;pose<3;pose++) emitPixels(line,stage,pose);
for(let stage=5;stage<=6;stage++) for(let pose=0;pose<3;pose++) emitPixels(0,stage,pose,true);
output.push('const lv_image_dsc_t digimon_sprites[3][7][3] = {');
for(let line=0;line<3;line++) {
  output.push('  {');
for(const form of names[line]) {
  const name=`line${line}_${form}`;
  output.push('    {');
  for(let pose=0;pose<3;pose++) output.push(`        {.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, .w = 32, .h = 28, .stride = 64}, .data_size = sizeof(${name}_${pose}), .data = ${name}_${pose}},`);
  output.push('    },');
}
  output.push('  },');
}
output.push('};','const lv_image_dsc_t digimon_branch_sprites[2][3] = {');
for(let stage=5;stage<=6;stage++) {
  output.push('  {');
  for(let pose=0;pose<3;pose++) {
    const id=`dark_${stage}_${pose}`;
    output.push(`    {.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, .w = 32, .h = 28, .stride = 64}, .data_size = sizeof(${id}), .data = ${id}},`);
  }
  output.push('  },');
}
output.push('};','');
const target=resolve(root,'assets/images/digimon_sprites.c'), source=output.join('\n');
if(process.argv.includes('--check')) {
  if(readFileSync(target,'utf8')!==source) throw new Error('Digimon sprite source is stale');
  console.log('Digimon sprites: PASS (3 lines + 2 branch forms, 3 poses, deterministic RGB565A8 assets)');
} else {writeFileSync(target,source);console.log('Generated 69 frames, 185472 bytes of RGB565A8 pixel data');}
