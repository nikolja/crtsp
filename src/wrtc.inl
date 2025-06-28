    inline static std::string content_code = { R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>WebRTC Stream</title>
            <style>
                body, html {
                    overscroll-behavior-y: contain;
                    margin: 0;
                    padding: 0;
                    overflow: hidden;
                }
                #video {
                    width: 100vw;
                    height: 100vh;
                    object-fit: contain;
                    background-color: black;
                }
                #overlay {
                    position: absolute;
                    top: 0; left: 0; right: 0; bottom: 0;
                    z-index: 5;
                    cursor: crosshair;
                    border: 2px dashed red;
                }
                #selection {
                    position: absolute;
                    border: 2px solid yellow;
                    background-color: rgba(255, 255, 0, 0.2);
                    display: none;
                    z-index: 10;
                    pointer-events: none;
                    opacity: 0;
                }
                #topLeftLayout {
                    position: absolute;
                    top: 1vh;
                    left: 1vw;
                    display: flex;
                    flex-direction: column;
                    gap: 1vh;
                    z-index: 20;
                }
                #buttonPanel {
                    display: flex;
                    flex-direction: column;
                    gap: 1vh;
                    z-index: 20;
                }
                #buttonPanel button {
                    font-size: 2vw;
                    padding: 2vh 2vw;
                    border: none;
                    border-radius: 8px;
                    background-color: rgba(0, 0, 0, 0.7);
                    color: white;
                    cursor: pointer;
                }
                #statusPanel {
                    display: flex;
                    gap: 1vh;
                    z-index: 20;
                }
                #statusPanel > div {
                    background: rgba(0,0,0,0.5);
                    color: white;
                    font-size: 1vw;
                    padding: 1vh 1vw;
                    border-radius: 1px;
                    pointer-events: none;
                }
                #fps, #status {
                    background: rgba(0,0,0,0.5);
                    color: white;
                    font-size: 1vw;
                    padding: 2vh 1vw;
                    border-radius: 1px;
                    pointer-events: none;
                }
            </style>
        </head>
        <body>
            <video id="video" autoplay playsinline muted></video>
            <div id="overlay"></div>
            <div id="topLeftLayout">
                <div id="statusPanel">
                    <div id="fps">FPS: --</div>
                    <div id="status">Online: <span id="status-text">🟢</span></div>
                </div>
                <div id="buttonPanel">
                    <button id="fullscreenBtn">⛶</button>
                </div>
            </div>

            <script>
                const video = document.getElementById('video');
                let peerId = localStorage.getItem('peer_id');
                let pc = new RTCPeerConnection({
                    iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
                });

                window.addEventListener('unload', () => {
                    const peerId = localStorage.getItem('peer_id');
                    if (!peerId) return;

                    navigator.sendBeacon('/api', JSON.stringify({
                        command: 'disconnect',
                        peer_id: peerId
                    }));
                });

                // status
                const statusText = document.getElementById("status-text");
                function updateConnectionStatus(state) {
                    switch (state) {
                        case "connected":
                            statusText.textContent = "🟢";
                            break;
                        case "disconnected":
                        case "failed":
                        case "closed":
                        case "frozen":
                            statusText.textContent = "🔴";
                            break;
                        default:
                            statusText.textContent = "🟡";
                            break;
                    }
                }

                pc.onconnectionstatechange = () => {
                    const state = pc.connectionState;
                    updateConnectionStatus(state);
                };

                pc.onicecandidate = async (event) => {
                    if (event.candidate) {
                        await fetch('/candidate?peer_id=' + peerId, {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify({
                                candidate: event.candidate.candidate,
                                sdpMLineIndex: event.candidate.sdpMLineIndex
                            })
                        });
                    }
                };

                pc.ontrack = (event) => {
                    if (event.track.kind === 'video') {
                        video.srcObject = event.streams[0];
                    }
                };

                async function start() {
                    pc.addTransceiver('video', { direction: 'recvonly' });

                    const offer = await pc.createOffer();
                    await pc.setLocalDescription(offer);

                    const response = await fetch('/offer', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json',
                            ...(peerId ? { 'X-Peer-ID': peerId } : {})
                        },
                        body: JSON.stringify({
                            type: 'offer',
                            sdp: pc.localDescription.sdp
                        })
                    });

                    if (response.headers.has('X-Peer-ID')) {
                        peerId = response.headers.get('X-Peer-ID');
                        localStorage.setItem('peer_id', peerId);
                    }

                    const data = await response.json();
                    await pc.setRemoteDescription(new RTCSessionDescription({
                        type: data.type,
                        sdp: data.sdp
                    }));

                    if (data.candidates) {
                        for (const ice of data.candidates) {
                            await pc.addIceCandidate(new RTCIceCandidate(ice));
                        }
                    }
                }

                start();

                // fps
                const fpsDisplay = document.getElementById("fps");
                let lastFpsTime = performance.now();
                let frameCounter = 0;
                function startFPSMonitor() {
                    function countFrame(now, metadata) {
                        frameCounter++;
                        const elapsed = now - lastFpsTime;
                        if (elapsed >= 1000) {
                            fpsDisplay.textContent = `FPS: ${frameCounter}`;
                            frameCounter = 0;
                            lastFpsTime = now;
                        }
                        video.requestVideoFrameCallback(countFrame);
                    }

                    if ('requestVideoFrameCallback' in HTMLVideoElement.prototype) {
                        video.requestVideoFrameCallback(countFrame);
                    } else {
                        function fallback() {
                            frameCounter++;
                            const now = performance.now();
                            const elapsed = now - lastFpsTime;
                            if (elapsed >= 1000) {
                                fpsDisplay.textContent = `FPS: ${frameCounter}`;
                                frameCounter = 0;
                                lastFpsTime = now;
                            }
                            requestAnimationFrame(fallback);
                        }
                        fallback();
                    }
                }

                startFPSMonitor();

                // align overlay to video
                function getVideoRect(video) {
                    const container = video.getBoundingClientRect();
                    const videoAspect = video.videoWidth / video.videoHeight;
                    const containerAspect = container.width / container.height;

                    let width, height, offsetX, offsetY;

                    if (videoAspect > containerAspect) {
                        width = container.width;
                        height = container.width / videoAspect;
                        offsetX = 0;
                        offsetY = (container.height - height) / 2;
                    } 
                    else {
                        width = container.height * videoAspect;
                        height = container.height;
                        offsetX = (container.width - width) / 2;
                        offsetY = 0;
                    }

                    return {
                        left: container.left + offsetX,
                        top: container.top + offsetY,
                        width: width,
                        height: height
                    };
                }

                function alignOverlay() {
                    const video = document.getElementById('video');
                    const overlay = document.getElementById('overlay');
                    const rect = getVideoRect(video);

                    overlay.style.top = rect.top + 'px';
                    overlay.style.left = rect.left + 'px';
                    overlay.style.width = rect.width + 'px';
                    overlay.style.height = rect.height + 'px';
                }

                video.addEventListener("loadedmetadata", alignOverlay);
                window.addEventListener('resize', alignOverlay);

                // align tl layout
                function alignTL() {
                    const panel = document.getElementById('topLeftLayout');
                    const vv = window.visualViewport;

                    panel.style.top = vv.offsetTop + vv.height * 0.01 + 'px';
                    panel.style.left = vv.offsetLeft + vv.width * 0.01 + 'px';
                    panel.style.gap = (vv.height * 0.01) + 'px';
                }
                function scaleElements(layout, scale=0.01) {
                    const width = window.visualViewport.width;
                    const height = window.visualViewport.height;

                    const leafElements = Array.from(layout.querySelectorAll('*'));
                    leafElements.forEach(el => {
                        el.style.fontSize = ((width > height ? width : height) * scale) + 'px';
                        el.style.padding = `${height * scale}px ${width * scale}px`;
                    });
                }
                function scalePanels(scale=0.01) {
                    const height = window.visualViewport.height;
                    const topLeftLayout = document.getElementById('topLeftLayout');
                    const panels = Array.from(topLeftLayout.children);

                    panels.forEach(panel => {
                        panel.style.gap = (height * scale) + 'px';
                    });
                }

                window.visualViewport?.addEventListener('scroll', e => {
                    alignTL();
                });
                window.visualViewport?.addEventListener('resize', e => {
                    alignTL();
                    scalePanels();
                    scaleElements(document.getElementById('buttonPanel'), 0.02);
                    scaleElements(document.getElementById('statusPanel'));
                });
                
                const overlay = document.getElementById('overlay');
                // process buttons:
                // fullscreen
                const btnFullscreen = document.getElementById('fullscreenBtn');
                function updateFullScreenBtnIcon() {
                    const isFullscreen = document.fullscreenElement != null;
                    btnFullscreen.textContent = isFullscreen ? '🔳' : '⛶';
                }
                btnFullscreen.addEventListener('click', () => {
                    if (!document.fullscreenElement) {
                        document.documentElement.requestFullscreen().catch(err => {
                            console.error(`Error attempting to enable full-screen mode: ${err.message}`);
                        });
                    } else {
                        document.exitFullscreen();
                    }
                });
                document.addEventListener('fullscreenchange', updateFullScreenBtnIcon);
                updateFullScreenBtnIcon(); // set on load
            </script>
        </body>
        </html>
    )" };