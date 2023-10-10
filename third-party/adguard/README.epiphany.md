# AdGuard

This directory contains an official BlockYouTubeAdsShortcut version, distributed at: https://github.com/AdguardTeam/BlockYouTubeAdsShortcut/

## Update process
1. Download https://github.com/AdguardTeam/BlockYouTubeAdsShortcut/blob/master/src/run-block-youtube.js and https://github.com/AdguardTeam/BlockYouTubeAdsShortcut/blob/master/src/index.js and concat them to youtube.js
2. Debrand script as requested by AdguardTeam. Comment the following line within the script:
    logo.innerHTML = '__logo_text__';

   Currently at line 396.
