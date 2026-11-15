<script>
  export let name;
  export let size = 18;
  const paths = {
    plus: 'M12 5v14M5 12h14',
    search: 'm21 21-5-5M19 11a8 8 0 1 1-16 0 8 8 0 0 1 16 0',
    library: 'M4 4v16M8 4v16M12 4v16m4-16 4 16',
    arrow: 'M5 12h14m-6-6 6 6-6 6',
    code: 'm8 6-6 6 6 6m8-12 6 6-6 6m-3-14-2 16',
    lock: 'M5 10h14v11H5zM8 10V6a4 4 0 0 1 8 0v4',
    copy: 'M9 9h12v12H9zM5 15H3V3h12v2',
    star: 'm12 3 2.8 5.7 6.2.9-4.5 4.4 1.1 6.2-5.6-3-5.6 3 1.1-6.2L3 9.6l6.2-.9z',
    download: 'M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5',
    globe: 'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3a17 17 0 0 1 0 18 17 17 0 0 1 0-18',
    clock: 'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M12 7v5l3 2',
    file: 'M14 2H5v20h14V7zM14 2v5h5M8 12h8m-8 4h6',
    check: 'm5 12 4 4L19 6',
    key: 'M15 3a6 6 0 1 1-4.5 10L3 21H1v-4l8-8a6 6 0 0 1 6-6',
    trash: 'M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7m4-7v7',
    fork: 'M7 3v12a4 4 0 0 0 4 4h2m4-16v5a4 4 0 0 1-4 4H7M4 3h6m4 0h6m-10 16h6',
    wrap: 'M3 6h18M3 12h14a4 4 0 0 1 0 8h-4m3-3-3 3 3 3M3 18h5'
  };
</script>
<svg width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d={paths[name] || paths.code}/></svg>
