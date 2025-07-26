class ChatApp {
    constructor() {
        this.ws = null;
        this.config = {};
        this.currentScene = 'day';

        // --- 核心状态变量 ---
        this.isSpeaking = false;      // 角色是否正在说话（整个对话过程）
        this.isTyping = false;        // 当前是否正在执行打字机效果
        this.conversationQueue = [];  // 对话片段队列
        this.currentSegment = null;   // 当前正在处理的对话片段
        this.typewriterTimeout = null;// 用于存储打字机的setTimeout ID，以便可以清除它

        this.initElements();
        this.connectWebSocket();
        this.setupInteraction();
    }

    initElements() {
        this.body = document.body;
        this.backgroundImg = document.querySelector('#background-layer img');
        this.characterSprite = document.getElementById('character-sprite');
        this.dialogText = document.getElementById('dialog-text');
        this.speakerName = document.getElementById('speaker-name');
        this.speakerIdentity = document.getElementById('speaker-identity');
        this.emotionTag = document.getElementById('emotion-tag');
        this.userInput = document.getElementById('user-input');
        this.sceneBtn = document.getElementById('scene-btn');
        this.timeInput = document.getElementById('time-input');
        this.timeBtn = document.getElementById('time-btn');
        this.thinkingIndicator = document.getElementById('thinking-indicator');
        this.continueIndicator = document.getElementById('continue-indicator');
        this.sfxVoice = document.getElementById('sfx-voice');
    }
    
    // 设置统一的交互入口
    setupInteraction() {
        // 主交互：点击对话框、按空格或回车
        const mainActionHandler = (e) => {
            // 防止在输入时触发
            if (document.activeElement === this.userInput || document.activeElement === this.timeInput) return;
            
            // 阻止默认行为（如空格滚动页面）
            if (e.key === ' ' || e.key === 'Enter') e.preventDefault();

            this.handleMainAction();
        };

        document.getElementById('dialog-box').addEventListener('click', mainActionHandler);
        document.addEventListener('keydown', (e) => {
            if (e.key === ' ' || e.key === 'Enter') mainActionHandler(e);
        });

        // 输入框和按钮的特定交互
        this.continueIndicator.addEventListener('click', (e) => {
            e.stopPropagation(); // 防止事件冒泡到dialog-box
            this.handleMainAction();
        });
        this.userInput.addEventListener('keypress', (e) => { 
            if (e.key === 'Enter') this.sendMessage(); 
        });
        
        this.sceneBtn.addEventListener('click', () => this.toggleScene());
        this.timeBtn.addEventListener('click', () => this.setTime());
    }

    // 【核心重构】主交互逻辑分发
    handleMainAction() {
        if (!this.isSpeaking) {
            // 如果不在对话中，且可以输入，则尝试发送消息
             this.sendMessage();
        } else if (this.isTyping) {
            // 如果正在打字，则跳过打字效果
            this.skipTypewriter();
        } else {
            // 如果不在打字（即一句话说完了），则推进到下一句
            this.displayNextSegment();
        }
    }

    connectWebSocket() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${wsProtocol}//${window.location.host}/websocket`);
        
        this.ws.onmessage = (event) => this.handleServerMessage(event.data);
        this.ws.onclose = () => { this.setDialogText('连接已断开。', '系统'); this.setSpeakingState(false); };
        this.ws.onerror = (error) => { console.error('WebSocket 错误:', error); this.setDialogText('连接发生错误！', '系统'); };
    }

    handleServerMessage(data) {
        const msg = JSON.parse(data);
        console.log("收到消息:", msg);

        switch (msg.type) {
            case 'server_ready':
                this.config = msg.payload;
                this.speakerName.textContent = this.config.character_name;
                this.updateBackground();
                this.updateCharacter('default');
                this.setDialogText('（连接成功。）');
                this.setSpeakingState(false);
                break;
            case 'ai_response':
            case 'narration':
                this.conversationQueue = msg.payload.segments;
                this.setSpeakingState(true);
                this.displayNextSegment();
                break;
            case 'error':
                 this.setDialogText(`${msg.payload.message} (${msg.payload.code})`, '错误');
                 this.setSpeakingState(false);
                 break;
        }
    }

    sendMessage() {
        const text = this.userInput.value.trim();
        if (text === '' || this.isSpeaking || this.ws.readyState !== WebSocket.OPEN) return;
        
        this.ws.send(JSON.stringify({ type: 'user_message', payload: { text } }));
        this.userInput.value = '';
        this.setSpeakingState(true, true); // 进入思考状态
    }

    // 【核心重构】对话推进逻辑
    displayNextSegment() {
        if (this.conversationQueue.length > 0) {
            this.currentSegment = this.conversationQueue.shift();
            this.showContinue(false);

            const fullText = this.currentSegment.action 
                ? `${this.currentSegment.action} ${this.currentSegment.text_cn}` 
                : this.currentSegment.text_cn;
            
            this.updateCharacter(this.currentSegment.expression);
            this.updateEmotionTag(this.currentSegment.expression); // 同步情绪标签
            this.playVoice(this.currentSegment.audio_url);
            
            this.typewriter(fullText, this.config.character_name);

        } else {
            // 所有对话结束
            this.setSpeakingState(false);
        }
    }

    // 【核心重构】打字机效果
    typewriter(text, speaker) {
        this.isTyping = true;
        this.speakerName.textContent = speaker;
        this.dialogText.textContent = '';
        let i = 0;
        const speed = 50;

        const type = () => {
            if (i < text.length) {
                this.dialogText.textContent += text.charAt(i);
                i++;
                this.typewriterTimeout = setTimeout(type, speed);
            } else {
                this.isTyping = false;
                this.showContinue(true); // 打字结束，显示“继续”图标
            }
        };
        type();
    }

    // 【核心新增】跳过打字机效果
    skipTypewriter() {
        if (!this.isTyping) return;

        this.isTyping = false;
        clearTimeout(this.typewriterTimeout);
        
        const fullText = this.currentSegment.action 
            ? `${this.currentSegment.action} ${this.currentSegment.text_cn}` 
            : this.currentSegment.text_cn;
        this.dialogText.textContent = fullText;
        
        this.showContinue(true); // 跳过完成，显示“继续”图标
    }

    setSpeakingState(isSpeaking, isThinking = false) {
        this.isSpeaking = isSpeaking;
        
        this.thinkingIndicator.classList.toggle('hidden', !isThinking);
        this.dialogText.classList.toggle('hidden', isThinking);
        this.continueIndicator.classList.add('hidden');

        if (isSpeaking) {
            this.body.classList.add('speaking');
        } else {
            this.body.classList.remove('speaking');
            this.emotionTag.classList.add('hidden'); // 对话结束时隐藏情绪标签
            this.currentSegment = null;
        }
    }
    
    updateCharacter(expression) {
        if (!this.config.ui_config) return;
        this.playSfx(expression);
        const emotionFile = this.config.ui_config.emotion_map[expression] || this.config.ui_config.emotion_map['default'] || 'neutral';
        const newSrc = `${this.config.ui_config.character_sprite_dir}${emotionFile}.png`;
        if (this.characterSprite.src && this.characterSprite.src.endsWith(newSrc)) return;
        this.characterSprite.style.opacity = '0';
        setTimeout(() => {
            this.characterSprite.src = newSrc;
            this.characterSprite.onload = () => { this.characterSprite.style.opacity = '1'; };
            this.characterSprite.onerror = () => {
                this.characterSprite.src = `${this.config.ui_config.character_sprite_dir}neutral.png`;
                this.characterSprite.style.opacity = '1';
            };
        }, 300);
    }
    
    // 其他辅助函数（无需修改）...
    toggleScene() { if (this.isSpeaking) return; this.currentScene = (this.currentScene === 'day') ? 'night' : 'day'; this.updateBackground(); const sceneName = (this.currentScene === 'day') ? '白天' : '黑夜'; this.ws.send(JSON.stringify({ type: 'system_command', payload: { command: 'set_scene', value: sceneName } })); this.setSpeakingState(true, true); }
    setTime() { if (this.isSpeaking) return; const time = this.timeInput.value.trim(); if (time === '') return; this.ws.send(JSON.stringify({ type: 'system_command', payload: { command: 'set_time', value: time } })); this.timeInput.value = ''; this.setSpeakingState(true, true); }
    updateBackground() { if (!this.config.ui_config) return; const newSrc = (this.currentScene === 'night') ? this.config.ui_config.background_night : this.config.ui_config.background_day; if (newSrc && !this.backgroundImg.src.endsWith(newSrc)) { this.backgroundImg.src = newSrc; } }
    updateEmotionTag(expression) { if (expression && expression !== 'neutral' && expression !== '旁白') { this.emotionTag.textContent = expression; this.emotionTag.classList.remove('hidden'); } else { this.emotionTag.classList.add('hidden'); } }
    playVoice(audioUrl) { if (audioUrl) { this.sfxVoice.src = audioUrl; this.sfxVoice.play().catch(e => console.error("语音播放失败:", e)); } }
    playSfx(expression) { if (!this.config.ui_config || !this.config.ui_config.emotion_sfx_map) return; const sfxFileName = this.config.ui_config.emotion_sfx_map[expression]; if (sfxFileName) { const sfxPath = `${this.config.ui_config.sfx_dir}${sfxFileName}`; const sfx = new Audio(sfxPath); sfx.play().catch(e => console.error("SFX 播放失败:", e, "路径:", sfxPath)); } }
    showContinue(show) { this.continueIndicator.classList.toggle('hidden', !show); }
    setDialogText(text, speaker) { if (!speaker && this.config) speaker = this.config.character_name; this.speakerName.textContent = speaker || ''; this.dialogText.textContent = text; this.emotionTag.classList.add('hidden'); }
}

document.addEventListener('DOMContentLoaded', () => { new ChatApp(); });