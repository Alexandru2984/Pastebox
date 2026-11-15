// Bound both work and rendered rows: large snippets fall back to two code views.
export function diffLines(before, after) {
  if (before.length > 200000 || after.length > 200000) return { mode:'large', rows:[] };
  const left = before.split('\n'), right = after.split('\n');
  if (left.length > 1200 || right.length > 1200) return { mode:'large', rows:[] };
  let prefix = 0, suffix = 0;
  while (prefix < left.length && prefix < right.length && left[prefix] === right[prefix]) prefix++;
  while (suffix < left.length-prefix && suffix < right.length-prefix && left[left.length-1-suffix] === right[right.length-1-suffix]) suffix++;
  const a = left.slice(prefix,left.length-suffix), b = right.slice(prefix,right.length-suffix);
  if ((a.length+1)*(b.length+1) > 250000) return { mode:'large', rows:[] };
  const width = b.length+1, matrix = new Uint16Array((a.length+1)*width);
  for (let i=a.length-1;i>=0;i--) for (let j=b.length-1;j>=0;j--) {
    matrix[i*width+j] = a[i] === b[j] ? matrix[(i+1)*width+j+1]+1 : Math.max(matrix[(i+1)*width+j],matrix[i*width+j+1]);
  }
  const rows = [];
  let leftNumber = 1, rightNumber = 1;
  function append(type,text) {
    rows.push({type,text,left:type === 'added' ? null : leftNumber++,right:type === 'removed' ? null : rightNumber++});
  }
  for (let i=0;i<prefix;i++) append('context',left[i]);
  let i=0,j=0;
  while (i<a.length || j<b.length) {
    if (i<a.length && j<b.length && a[i] === b[j]) {append('context',a[i]);i++;j++;}
    else if (i<a.length && (j===b.length || matrix[(i+1)*width+j] >= matrix[i*width+j+1])) append('removed',a[i++]);
    else append('added',b[j++]);
  }
  for (let k=left.length-suffix;k<left.length;k++) append('context',left[k]);
  return rows.length > 2000 ? {mode:'large',rows:[]} : {mode:'diff',rows};
}
