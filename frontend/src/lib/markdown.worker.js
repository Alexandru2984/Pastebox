import { parseMarkdown } from './markdown-parser.js';
self.onmessage = ({data}) => {
  try {self.postMessage({html:parseMarkdown(data.content)});}
  catch {self.postMessage({error:'This Markdown cannot be previewed within the size/complexity limits. Your source is unchanged.'});}
};
self.postMessage({ready:true});
