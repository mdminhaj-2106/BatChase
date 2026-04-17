var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var F84;
(function (F84) {
    var Boot = (function (_super) {
        __extends(Boot, _super);
        function Boot() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        Boot.prototype.init = function () {
            this.input.maxPointers = 2;
            this.scale.pageAlignHorizontally = true;
            this.scale.pageAlignVertically = true;
            this.scale.scaleMode = Phaser.ScaleManager.SHOW_ALL;
            this.scale.fullScreenScaleMode = Phaser.ScaleManager.SHOW_ALL;
            F84.PhaserText.override();
        };
        Boot.prototype.preload = function () {
            console.log(F84.Game.Instance.locale);
            this.load.json('strings', './locale/' + F84.Game.Instance.locale + '/strings.json');
            this.load.json('gameConfig', './assets/json/game-config.json');
            this.load.json('chunks', './assets/json/chunks.json');
            this.load.image('rotateDevice', './assets/images/system/orientation.png');
            this.load.image('loadBG', './assets/images/ui/lbm-ui-loading-bg.png');
            this.load.image('loadLogo', './locale/' + F84.Game.Instance.locale + '/images/lbm-ui-logo-splash.png');
            this.load.image('loadBar', './assets/images/ui/lbm-ui-loading-box-container.png');
            this.load.image('loadFill', './assets/images/ui/lbm-ui-loading-box-fill.png');
            this.load.image('uiLegoBatmanLogo', './assets/images/ui/lbm-ui-logo-lego-batman.png');
            this.load.image('uiBatman80Logo', './assets/images/ui/lbm-ui-logo-batman-80.png');
        };
        Boot.prototype.create = function () {
            F84.Orientation.rescale(this, F84.OrientationType.LANDSCAPE, F84.ScaleBy.HEIGHT);
            F84.GameConfig.loadConfigFile(this.cache, "gameConfig");
            this.game.state.start('Preloader');
        };
        return Boot;
    }(Phaser.State));
    F84.Boot = Boot;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var CustomLoader = (function (_super) {
        __extends(CustomLoader, _super);
        function CustomLoader(game) {
            return _super.call(this, game) || this;
        }
        CustomLoader.prototype.webfont = function (key, fontName, overwrite) {
            if (overwrite === void 0) { overwrite = false; }
            this.addToFileList('webfont', key, fontName);
            return this;
        };
        CustomLoader.prototype.loadFile = function (file) {
            _super.prototype.loadFile.call(this, file);
            if (file.type == 'webfont') {
                var _this = this;
                var font = new FontFaceObserver(file.url);
                font.load(null, 10000).then(function () {
                    _this.asyncComplete(file);
                }, function () {
                    _this.asyncComplete(file, 'Error loading font ' + file.url);
                });
            }
        };
        return CustomLoader;
    }(Phaser.Loader));
    F84.CustomLoader = CustomLoader;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var DraggableGroup = (function (_super) {
        __extends(DraggableGroup, _super);
        function DraggableGroup(game, horizontalScroll, verticalScroll) {
            var _this = _super.call(this, game) || this;
            _this.vx = 0;
            _this.vy = 0;
            _this.horizontalScroll = horizontalScroll ? 1 : 0;
            _this.verticalScroll = verticalScroll ? 1 : 0;
            _this.dragPoints = [];
            _this.dragDistance = 0;
            _this.inputEnabled = true;
            _this.dragObjects = [];
            _this.autoScrolling = false;
            _this.autoScrollSpeed = 8;
            _this.minX = -Number.MAX_VALUE;
            _this.maxX = Number.MAX_VALUE;
            _this.minY = -Number.MAX_VALUE;
            _this.maxY = Number.MAX_VALUE;
            _this.softBounds = false;
            _this.onDragEnd = new Phaser.Signal();
            return _this;
        }
        DraggableGroup.prototype.update = function () {
            _super.prototype.update.call(this);
            var dt = this.game.time.physicsElapsed;
            this.x += this.vx * dt;
            this.y += this.vy * dt;
            this.vx -= this.vx * 8 * dt;
            this.vy -= this.vy * 8 * dt;
            if (this.autoScrolling) {
                this.x -= (this.x - this.autoScrollPoint.x) * this.autoScrollSpeed * dt;
                this.y -= (this.y - this.autoScrollPoint.y) * this.autoScrollSpeed * dt;
            }
            if (this.softBounds) {
                if (this.x < this.minX)
                    this.x += (this.minX - this.x) * 15 * dt;
                if (this.x > this.maxX)
                    this.x += (this.maxX - this.x) * 15 * dt;
                if (this.y < this.minY)
                    this.y += (this.minY - this.y) * 15 * dt;
                if (this.y > this.maxY)
                    this.y += (this.maxY - this.y) * 15 * dt;
            }
            else {
                if (this.x < this.minX)
                    this.x = this.minX;
                if (this.x > this.maxX)
                    this.x = this.maxX;
                if (this.y < this.minY)
                    this.y = this.minY;
                if (this.y > this.maxY)
                    this.y = this.maxY;
            }
            this.dragPoints.pop();
        };
        DraggableGroup.prototype.enable = function () {
            for (var _i = 0, _a = this.dragObjects; _i < _a.length; _i++) {
                var obj = _a[_i];
                obj.input.enableDrag();
            }
        };
        DraggableGroup.prototype.disable = function () {
            for (var _i = 0, _a = this.dragObjects; _i < _a.length; _i++) {
                var obj = _a[_i];
                obj.input.disableDrag();
            }
        };
        DraggableGroup.prototype.dragStart = function (obj, ptr, x, y) {
            this.dragStartPoint = new Phaser.Point(ptr.x, ptr.y);
            this.vx = 0;
            this.vy = 0;
            this.dragDistance = 0;
            this.stopScrolling();
        };
        DraggableGroup.prototype.dragUpdate = function (obj, ptr, x, y) {
            if (this.game.input.totalActivePointers > 1)
                return;
            if (this.inputEnabled) {
                if (this.dragStartPoint === undefined)
                    this.dragStartPoint = new Phaser.Point(ptr.x, ptr.y);
                var dragUpdatePoint = new Phaser.Point(ptr.x, ptr.y);
                var delta = dragUpdatePoint.subtract(this.dragStartPoint.x, this.dragStartPoint.y);
                this.position = this.position.add(delta.x * this.horizontalScroll, delta.y * this.verticalScroll);
                if (this.softBounds) {
                    if (this.x < this.minX) {
                        this.x += (this.minX - this.x) * 0.1;
                        delta.x *= 0.4;
                    }
                    if (this.x > this.maxX) {
                        this.x += (this.maxX - this.x) * 0.1;
                        delta.x *= 0.4;
                    }
                    if (this.y < this.minY) {
                        this.y += (this.minY - this.y) * 0.1;
                        delta.y *= 0.4;
                    }
                    if (this.y > this.maxY) {
                        this.y += (this.maxY - this.y) * 0.1;
                        delta.y *= 0.4;
                    }
                }
                else {
                    if (this.x < this.minX)
                        this.x = this.minX;
                    if (this.x > this.maxX)
                        this.x = this.maxX;
                    if (this.y < this.minY)
                        this.y = this.minY;
                    if (this.y > this.maxY)
                        this.y = this.maxY;
                }
                this.dragDistance += Math.abs(delta.x * this.horizontalScroll);
                this.dragDistance += Math.abs(delta.y * this.verticalScroll);
                this.dragPoints.unshift(delta);
                this.dragPoints.unshift(delta);
                if (this.dragPoints.length > 8)
                    this.dragPoints.pop();
                this.dragStartPoint.x = ptr.x;
                this.dragStartPoint.y = ptr.y;
            }
        };
        DraggableGroup.prototype.dragStop = function () {
            this.dragStartPoint = undefined;
            if (this.dragPoints.length > 0 && this.inputEnabled) {
                var avgX = 0;
                var avgY = 0;
                for (var _i = 0, _a = this.dragPoints; _i < _a.length; _i++) {
                    var delta = _a[_i];
                    avgX += delta.x;
                    avgY += delta.y;
                }
                avgX /= this.dragPoints.length;
                avgY /= this.dragPoints.length;
                this.vx = avgX * this.horizontalScroll / this.game.time.physicsElapsed;
                this.vy = avgY * this.verticalScroll / this.game.time.physicsElapsed;
            }
            this.dragPoints = [];
            this.onDragEnd.dispatch();
        };
        DraggableGroup.prototype.scrollTo = function (x, y) {
            this.autoScrolling = true;
            this.autoScrollPoint = new Phaser.Point(x, y);
        };
        DraggableGroup.prototype.stopScrolling = function () {
            this.autoScrolling = false;
        };
        DraggableGroup.prototype.enableDragging = function (obj) {
            obj.inputEnabled = true;
            obj.input.draggable = true;
            obj.input.allowHorizontalDrag = false;
            obj.input.allowVerticalDrag = false;
            obj.events.onDragStart.add(this.dragStart, this);
            obj.events.onDragUpdate.add(this.dragUpdate, this);
            obj.events.onDragStop.add(this.dragStop, this);
            this.dragObjects.push(obj);
        };
        DraggableGroup.prototype.destroy = function (destroyChildren, soft) {
            this.dragObjects.forEach(function (element) {
                element.events.destroy();
                element.destroy(true);
            });
            _super.prototype.destroy.call(this, destroyChildren, soft);
            this.onDragEnd.dispose();
            this.dragObjects = null;
            this.dragPoints = null;
        };
        return DraggableGroup;
    }(Phaser.Group));
    F84.DraggableGroup = DraggableGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Game = (function (_super) {
        __extends(Game, _super);
        function Game() {
            var _this = _super.call(this, 1080, 1080, Phaser.CANVAS, "", null, true) || this;
            _this._locale = "en-us";
            _this._defaultWidth = 1080;
            _this._defaultHeight = 1080;
            Game._instance = _this;
            _this.playerData = new F84.PlayerData("LegoBatman");
            var net = new Phaser.Net(_this);
            var localeQuery = net.getQueryString("locale");
            var validLocales = {
                "ar-ar": "ar-ar",
                "cz-cz": "cz-cz",
                "da-dk": "da-dk",
                "da-da": "da-dk",
                "de-de": "de-de",
                "en-us": "en-us",
                "es-es": "es-es",
                "es-mx": "es-mx",
                "fr-fr": "fr-fr",
                "hu-hu": "hu-hu",
                "it-it": "it-it",
                "ja-jp": "ja-jp",
                "ja-ja": "ja-jp",
                "ko-ko": "ko-ko",
                "nl-nl": "nl-nl",
                "no-no": "no-no",
                "pl-pl": "pl-pl",
                "pt-br": "pt-br",
                "ru-ru": "ru-ru",
                "sv-sv": "sv-sv",
                "tr-tr": "tr-tr",
                "zh-cn": "zh-cn"
            };
            if (Object.keys(validLocales).indexOf(localeQuery) === -1) {
                console.log(localeQuery + ' DOES NOT EXIST. DEFAULTING TO \'en-us\'');
                localeQuery = 'en-us';
            }
            else
                localeQuery = validLocales[localeQuery];
            if (!F84.ObjectUtils.isEmpty(localeQuery)) {
                _this._locale = localeQuery;
            }
            else {
                if (window.frameElement != null) {
                    var attribute = window.frameElement.attributes.getNamedItem("data-locale");
                    if (attribute != null) {
                        _this._locale = attribute.value.toLowerCase();
                    }
                }
            }
            _this.preserveDrawingBuffer = true;
            _this.state.add('Boot', F84.Boot);
            _this.state.add('Preloader', F84.Preloader);
            _this.state.add('MainMenu', F84.MainMenu);
            _this.state.add('StoryIntro', F84.StoryIntro);
            _this.state.add('ShopState', F84.ShopState);
            _this.state.add('GameState', F84.GameState);
            _this.state.add('RecapScreen', F84.RecapScreen);
            _this.state.start('Boot');
            return _this;
        }
        Object.defineProperty(Game.prototype, "DefaultWidth", {
            get: function () {
                return this._defaultWidth;
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(Game.prototype, "DefaultHeight", {
            get: function () {
                return this._defaultHeight;
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(Game, "Instance", {
            get: function () {
                return this._instance;
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(Game.prototype, "locale", {
            get: function () {
                return this._locale;
            },
            enumerable: true,
            configurable: true
        });
        Game.prototype.getBounds = function () {
            return new Phaser.Rectangle(0, 0, this.width, this.height);
        };
        Game.prototype.boot = function () {
            _super.prototype.boot.call(this);
            this.load = new F84.CustomLoader(this);
        };
        return Game;
    }(Phaser.Game));
    F84.Game = Game;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameColors = (function () {
        function GameColors() {
        }
        GameColors.WHITE = "#ffffff";
        GameColors.ORANGE = "#ffd800";
        GameColors.YELLOW = "#ffffff";
        GameColors.BROWN = "#ffffff";
        GameColors.DARK_GREY_HEX = 0x646464;
        return GameColors;
    }());
    F84.GameColors = GameColors;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ResizableState = (function (_super) {
        __extends(ResizableState, _super);
        function ResizableState() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        ResizableState.prototype.preload = function () {
            var _this = this;
            _super.prototype.preload.call(this, this.game);
            this.scale.setResizeCallback(function (scale, parentBounds) {
                if (_this._lastResizeBounds == null
                    || parentBounds.x != _this._lastResizeBounds.x
                    || parentBounds.y != _this._lastResizeBounds.y
                    || parentBounds.width != _this._lastResizeBounds.width
                    || parentBounds.height != _this._lastResizeBounds.height) {
                    _this._lastResizeBounds = parentBounds.clone();
                    F84.Orientation.rescale(_this, F84.OrientationType.LANDSCAPE, F84.ScaleBy.HEIGHT);
                }
            }, this);
            this.scale.onSizeChange.add(function () {
                _this.resize();
            }, this);
        };
        ResizableState.prototype.resize = function () {
            _super.prototype.resize.call(this, F84.Game.Instance.width, F84.Game.Instance.height);
        };
        ResizableState.prototype.loadImages = function (prefix, map, suffix) {
            if (suffix === void 0) { suffix = '.png'; }
            for (var name_1 in map) {
                this.load.image(name_1, prefix + map[name_1] + suffix);
            }
        };
        ResizableState.prototype.loadImagesVerbatim = function (prefix, names, suffix) {
            if (suffix === void 0) { suffix = '.png'; }
            for (var _i = 0, names_1 = names; _i < names_1.length; _i++) {
                var name_2 = names_1[_i];
                this.load.image(name_2, prefix + name_2 + suffix);
            }
        };
        ResizableState.prototype.shutdown = function () {
            this.scale.onSizeChange.removeAll(this);
        };
        return ResizableState;
    }(Phaser.State));
    F84.ResizableState = ResizableState;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameComplete = (function (_super) {
        __extends(GameComplete, _super);
        function GameComplete() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        GameComplete.prototype.create = function () {
            var _this = this;
            this.game.input.onDown.add(function () { return F84.Fullscreen.startFullscreen(_this.game); });
            F84.Music.switchMusic(this.game, 'menuMusic');
            var background = this.add.sprite(0, 0, 'gameCompleteBG');
            var characters = this.add.sprite(0, 0, 'gameCompleteCharacters');
            var box = this.add.sprite(0, 0, 'storyTextBox');
            var playButton = this.add.button(0, 0, 'levelPlayBtn', this.play, this);
            var style = { fill: 'white', fontSize: 26, font: F84.Fonts.DefaultFont, wordWrap: true, wordWrapWidth: 700, align: 'center' };
            var text = this.add.text(0, 0, F84.Strings.get('GameCompleteText'), style);
            this._onResize = function () {
                F84.SpriteUtils.aspectFill(background, _this.world.bounds);
                background.alignIn(_this.world.bounds, Phaser.CENTER);
                characters.alignIn(_this.world.bounds, Phaser.CENTER, 0, -20);
                box.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 0, -40);
                text.alignIn(box, Phaser.CENTER);
                playButton.alignIn(_this.world.bounds, Phaser.BOTTOM_RIGHT, -20, -20);
            };
            this.resize();
        };
        GameComplete.prototype.resize = function () {
            this._onResize();
        };
        GameComplete.prototype.play = function () {
            this.game.sound.play('button');
            this.game.state.start('MainMenu');
        };
        return GameComplete;
    }(F84.ResizableState));
    F84.GameComplete = GameComplete;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameConfig = (function () {
        function GameConfig() {
        }
        GameConfig.loadConfigFile = function (cache, key) {
            this.json = cache.getJSON(key);
        };
        GameConfig.get = function (id) {
            if (this.json === undefined)
                throw new Error('Config file not loaded');
            if (this.json[id] === undefined)
                throw new Error("Key '" + id + "' does not exist in config file");
            return this.json[id];
        };
        return GameConfig;
    }());
    F84.GameConfig = GameConfig;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var MainMenu = (function (_super) {
        __extends(MainMenu, _super);
        function MainMenu() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        MainMenu.prototype.create = function () {
            var _this = this;
            this.game.input.onDown.add(function () { return F84.Fullscreen.startFullscreen(_this.game); });
            F84.Music.switchMusic(this.game, 'menuMusic');
            var bounds = F84.UIUtils.getMobileBounds(this.world.bounds);
            var makeRain = function (x) {
                var rain = new Phaser.Particles.Arcade.Emitter(_this.game, 0, -200, 100);
                _this.particles.add(rain);
                rain.flow(1200, 50, 1);
                rain.makeParticles(['uiSplashRain1', 'uiSplashRain2']);
                rain.setYSpeed(2000, 4000);
                rain.setXSpeed(-1, 1);
                rain.setRotation(0, 0);
                var update = rain.update;
                rain.update = function () {
                    update.call(rain);
                    rain.emitX = _this.rnd.between(x, x + 500);
                };
            };
            var bg = this.add.sprite(0, 0, 'uiSplashBG');
            makeRain(bounds.centerX + 90);
            var car = this.add.sprite(0, 0, 'uiSplashCar');
            bg.alignIn(this.world.bounds, Phaser.CENTER);
            car.alignIn(bg, Phaser.CENTER, 200, 250);
            var carTween = this.add.tween(car).from({ x: car.x - 600, y: car.y - 300 }, 500, Phaser.Easing.Quadratic.Out, false, 500);
            this.add.tween(car).from({ alpha: 0 }, 300, Phaser.Easing.Linear.None, true, 500);
            makeRain(bounds.centerX - 520);
            var logoLego = this.add.sprite(0, 0, 'uiLegoBatmanLogo');
            var logo = this.add.sprite(0, 0, 'uiSplashLogo');
            var batman80Logo = this.add.sprite(0, 0, 'uiBatman80Logo');
            var scoreGroup = this.add.group();
            var hiScoreBG = new F84.NineSlicedSprite(this.game, 0, 0, 'uiSplashHiScore', 300, 64, 0, 30, 0, 63);
            this.game.add.existing(hiScoreBG);
            scoreGroup.add(hiScoreBG);
            var hiScoreHeaderStyle = { fontSize: 20, fill: F84.GameColors.ORANGE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var hiScoreHeader = this.add.text(0, 0, '', hiScoreHeaderStyle, scoreGroup);
            hiScoreHeader.setText(F84.Strings.get('HighScore'));
            hiScoreHeader.alignIn(hiScoreBG, Phaser.LEFT_CENTER, -65, 3);
            var hiScoreStyle = { fontSize: 34, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var score = this.game.add.text(20, 50, '' + F84.PlayerData.Instance.saveData.highScore, hiScoreStyle, scoreGroup);
            score.alignTo(hiScoreHeader, Phaser.RIGHT_CENTER, 2, 0);
            var soundButton, resetButton, backButton, settingsButton;
            if (!F84.LegoUtils.onLegoSite()) {
                soundButton = F84.UIFactory.createLargeSoundButton(this.game);
                soundButton.scale.set(0.72);
                resetButton = this.add.button(-125, 0, 'uiRestartButton', function () { return _this.reset(); });
                if (!F84.PlayerData.Instance.saveData.gameStarted) {
                    resetButton.inputEnabled = false;
                    resetButton.loadTexture('uiRestartInactiveButton');
                }
            }
            else {
                backButton = this.add.button(0, 0, 'uiBackButton', function () {
                    window.location.href = F84.GameConfig.get('legoButtonLink');
                    _this.game.sound.play('button');
                }, this);
                settingsButton = this.add.button(0, 0, 'uiSettingsButton', this.openSettings, this);
            }
            var shopButton = this.add.button(0, 0, 'uiShopButton', this.shop, this);
            if (!F84.PlayerData.Instance.saveData.gameStarted) {
                shopButton.inputEnabled = false;
                shopButton.loadTexture('uiShopInactiveButton');
            }
            var playButton = this.add.button(0, 0, 'uiPlayButton', this.play, this);
            var style = { fontSize: 20, font: 'Arial', fontWeight: 'bold', fill: F84.GameColors.WHITE, align: 'center' };
            var policyGroup = this.add.group();
            var legalString = F84.Strings.get('DCLegalLines');
            if (F84.LegoUtils.onLegoSite()) {
                var separator = this.add.text(0, 0, ' | ', style, policyGroup);
                var privacyPolicy = this.add.text(0, 0, '', style, policyGroup);
                privacyPolicy.setText(F84.Strings.get('PrivacyPolicy'));
                privacyPolicy.inputEnabled = true;
                privacyPolicy.events.onInputOver.add(function () { return document.body.style.cursor = 'pointer'; }, this);
                privacyPolicy.events.onInputOut.add(function () { return document.body.style.cursor = 'default'; }, this);
                privacyPolicy.events.onInputUp.add(function () { return window.open(F84.GameConfig.get('privacyLink')); }, this);
                privacyPolicy.alignTo(separator, Phaser.LEFT_CENTER);
                var cookiePolicy = this.add.text(0, 0, '', style, policyGroup);
                cookiePolicy.setText(F84.Strings.get('CookiePolicy'));
                cookiePolicy.inputEnabled = true;
                cookiePolicy.events.onInputOver.add(function () { return document.body.style.cursor = 'pointer'; }, this);
                cookiePolicy.events.onInputOut.add(function () { return document.body.style.cursor = 'default'; }, this);
                cookiePolicy.events.onInputUp.add(function () { return window.open(F84.GameConfig.get('cookieLink')); }, this);
                cookiePolicy.alignTo(separator, Phaser.RIGHT_CENTER);
                legalString += '\n' + F84.Strings.get('LegoLegalLines');
            }
            style.fontSize = 16;
            style.fontWeight = '';
            var legalLines = this.add.text(0, 0, legalString, style);
            legalLines.lineSpacing = -5;
            this._onResize = function () {
                bounds = F84.UIUtils.getMobileBounds(_this.world.bounds);
                F84.SpriteUtils.aspectFill(bg, _this.world.bounds);
                bg.alignIn(_this.world.bounds, Phaser.CENTER);
                car.alignIn(bg, Phaser.CENTER, 200, 250);
                logoLego.alignIn(_this.world.bounds, Phaser.TOP_CENTER, -9.5, -10);
                logo.alignIn(_this.world.bounds, Phaser.TOP_CENTER, -0.5, -121);
                batman80Logo.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, -0.5, -88);
                hiScoreBG.resize(score.right - hiScoreBG.x + 10, 64);
                scoreGroup.alignTo(logo, Phaser.BOTTOM_CENTER, -17, -81);
                shopButton.alignIn(bounds, Phaser.BOTTOM_LEFT, -120, -5);
                playButton.alignIn(bounds, Phaser.BOTTOM_RIGHT, -10, -5);
                if (!F84.LegoUtils.onLegoSite()) {
                    soundButton.alignIn(bounds, Phaser.TOP_RIGHT, -56, -17);
                    resetButton.alignIn(bounds, Phaser.TOP_LEFT, -114, -21);
                }
                else {
                    settingsButton.alignIn(bounds, Phaser.TOP_RIGHT, -56, -17);
                    backButton.alignIn(bounds, Phaser.TOP_LEFT, -114, -21);
                }
                if (_this.settingsGroup)
                    _this.settingsGroup.alignIn(bounds, Phaser.CENTER);
                policyGroup.alignTo(batman80Logo, Phaser.BOTTOM_CENTER, 0, 10);
                legalLines.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 0, -10);
            };
            this.resize();
            carTween.start();
        };
        MainMenu.prototype.resize = function () {
            this._onResize();
        };
        MainMenu.prototype.play = function () {
            var _this = this;
            this.game.sound.play('button');
            if (!F84.PlayerData.Instance.saveData.gameStarted) {
                F84.PlayerData.Instance.saveData.gameStarted = true;
                F84.PlayerData.Instance.save();
                new F84.UITransition(this.game, function () { return _this.game.state.start('StoryIntro'); });
            }
            else {
                F84.GameState.shownTutorial = true;
                new F84.UITransition(this.game, function () { return _this.game.state.start('GameState'); });
            }
        };
        MainMenu.prototype.shop = function () {
            var _this = this;
            this.game.sound.play('button');
            new F84.UITransition(this.game, function () { return _this.game.state.start('ShopState'); });
        };
        MainMenu.prototype.openSettings = function () {
            var _this = this;
            this.game.sound.play('button');
            var reset = function () { return _this.reset(_this.openSettings, _this); };
            this.settingsGroup = new F84.SettingsGroup(this.game, reset);
            this.add.existing(this.settingsGroup);
            this.settingsGroup.alignIn(this.world.bounds, Phaser.CENTER);
        };
        MainMenu.prototype.reset = function (onClose, onCloseContext) {
            var _this = this;
            this.game.sound.play('button');
            var config = {
                messageText: F84.Strings.get('ResetGameMessage'),
                yesButtonText: F84.Strings.get('ResetYes'),
                noButtonText: F84.Strings.get('ResetNo'),
                yesFunction: function () {
                    F84.PlayerData.Instance.reset();
                    _this.game.state.start('MainMenu');
                },
                yesContext: this,
                noFunction: function () {
                    if (onClose != null)
                        onClose.call(onCloseContext);
                }
            };
            var yesNoDialogueGroup = new F84.YesNoDialogueGroup(this.game, config);
            this.add.existing(yesNoDialogueGroup);
        };
        return MainMenu;
    }(F84.ResizableState));
    F84.MainMenu = MainMenu;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var NineSlicedSprite = (function (_super) {
        __extends(NineSlicedSprite, _super);
        function NineSlicedSprite(game, x, y, key, width, height, top, right, bottom, left) {
            var _this = this;
            var spriteRef = new Phaser.Sprite(game, 0, 0, key);
            var renderTexture = new Phaser.RenderTexture(game, width, height);
            _this = _super.call(this, game, x, y, renderTexture) || this;
            _this._baseTexture = spriteRef.texture.baseTexture;
            _this._renderTexture = renderTexture;
            _this._top = top;
            _this._bottom = bottom;
            _this._right = right;
            _this._left = left;
            _this.resize(width, height);
            spriteRef.destroy();
            return _this;
        }
        NineSlicedSprite.prototype.resize = function (width, height) {
            if (this._prevHeight == height && this._prevWidth == width)
                return;
            this._prevHeight = height;
            this._prevWidth = width;
            this._renderTexture = new Phaser.RenderTexture(this.game, width, height);
            this.loadTexture(this._renderTexture);
            this.renderPiece(0, 0, new PIXI.Rectangle(0, 0, this._left, this._top));
            this.renderPiece(width - this._right, 0, new PIXI.Rectangle(this._baseTexture.width - this._right, 0, this._right, this._top));
            this.renderStretchPiece(this._left, 0, new PIXI.Rectangle(this._left, 0, this._baseTexture.width - this._left - this._right, this._top), width - this._left - this._right, this._top);
            this.renderPiece(0, height - this._bottom, new PIXI.Rectangle(0, this._baseTexture.height - this._bottom, this._left, this._bottom));
            this.renderPiece(width - this._right, height - this._bottom, new PIXI.Rectangle(this._baseTexture.width - this._right, this._baseTexture.height - this._bottom, this._left, this._bottom));
            this.renderStretchPiece(this._left, height - this._bottom, new PIXI.Rectangle(this._left, this._baseTexture.height - this._bottom, this._baseTexture.width - this._left - this._right, this._bottom), width - this._left - this._right, this._bottom);
            this.renderStretchPiece(0, this._top, new PIXI.Rectangle(0, this._top, this._left, this._baseTexture.height - this._top - this._bottom), this._left, height - this._top - this._bottom);
            this.renderStretchPiece(width - this._right, this._top, new PIXI.Rectangle(this._baseTexture.width - this._right, this._top, this._right, this._baseTexture.height - this._top - this._bottom), this._right, height - this._top - this._bottom);
            this.renderStretchPiece(this._left, this._top, new PIXI.Rectangle(this._left, this._top, this._baseTexture.width - this._left - this._right, this._baseTexture.height - this._top - this._bottom), width - this._left - this._right, height - this._top - this._bottom);
        };
        NineSlicedSprite.prototype.renderPiece = function (x, y, frame) {
            var sprite = new Phaser.Sprite(this.game, 0, 0, new PIXI.Texture(this._baseTexture, frame));
            this._renderTexture.renderXY(sprite, x, y);
            sprite.destroy();
        };
        NineSlicedSprite.prototype.renderStretchPiece = function (x, y, frame, width, height) {
            var sprite = new Phaser.Sprite(this.game, 0, 0, new PIXI.Texture(this._baseTexture, frame));
            sprite.width = width;
            sprite.height = height;
            this._renderTexture.renderXY(sprite, x, y);
            sprite.destroy();
        };
        NineSlicedSprite.prototype.destroy = function (destroyChildren) {
            _super.prototype.destroy.call(this, destroyChildren);
        };
        return NineSlicedSprite;
    }(Phaser.Sprite));
    F84.NineSlicedSprite = NineSlicedSprite;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var SaveData = (function () {
        function SaveData() {
            this.gameStarted = false;
            this.highScore = 0;
            this.studs = 0;
            this.upgrades = {
                health: 0,
                stud: 0,
                magnet: 0,
                rocketBooster: 0,
                homingRocket: 0
            };
        }
        return SaveData;
    }());
    F84.SaveData = SaveData;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PlayerData = (function () {
        function PlayerData(gameId) {
            this._gameId = gameId;
            this.load();
            PlayerData.Instance = this;
        }
        Object.defineProperty(PlayerData.prototype, "localStorageKey", {
            get: function () {
                return this._gameId + "_SaveData";
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(PlayerData.prototype, "saveData", {
            get: function () {
                return this._saveData;
            },
            enumerable: true,
            configurable: true
        });
        PlayerData.prototype.load = function () {
            var base64String = localStorage.getItem(this.localStorageKey);
            if (base64String != '' && base64String != null) {
                this._saveData = new F84.SaveData();
                var jsonText = window.atob(base64String);
                var data = JSON.parse(jsonText);
                for (var key in data) {
                    this._saveData[key] = data[key];
                }
            }
            else {
                this._saveData = new F84.SaveData();
            }
        };
        PlayerData.prototype.save = function () {
            var jsonString = JSON.stringify(this._saveData);
            var base64String = window.btoa(jsonString);
            localStorage.setItem(this.localStorageKey, base64String);
        };
        PlayerData.prototype.reset = function () {
            localStorage.removeItem(this.localStorageKey);
            this._saveData = new F84.SaveData();
        };
        return PlayerData;
    }());
    F84.PlayerData = PlayerData;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Preloader = (function (_super) {
        __extends(Preloader, _super);
        function Preloader() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        Object.defineProperty(Preloader.prototype, "load", {
            get: function () {
                return this.game.load;
            },
            enumerable: true,
            configurable: true
        });
        Preloader.prototype.preload = function () {
            _super.prototype.preload.call(this);
            var preloader = this._preloader = new F84.PreloaderGroup(this.game);
            this.add.existing(preloader);
            this.load.webfont('gobold', 'gobold-bold-italic');
            this.load.webfont('roboto', 'roboto-medium');
            this.loadImages('./assets/images/temp/', {
                whiteSquare: 'white-square'
            });
            this.loadImages('./assets/images/ui/lbm-', {
                uiBackButton: 'btn-back-small',
                uiHomeButton: 'btn-home',
                uiSettingsButton: 'btn-settings',
                uiPauseHelpButton: 'btn-menu-help',
                uiPauseHomeButton: 'btn-menu-home',
                uiSoundButton: 'btn-menu-sound',
                uiSoundButtonOff: 'btn-menu-sound-off',
                uiPlayButton: 'btn-play',
                uiReplayButton: 'btn-replay',
                uiRestartButton: 'btn-restart',
                uiRestartInactiveButton: 'btn-restart-inactive',
                uiShopButton: 'btn-shop',
                uiShopInactiveButton: 'btn-shop-inactive',
                uiSoundButtonSmall: 'btn-sound-small',
                uiSoundButtonSmallOff: 'btn-sound-small-off',
                uiHeaderGlow: 'ui-header-glow',
                uiPacingGlow: 'ui-pacing-glow',
                uiStoreGradient: 'ui-store-header-gradient',
                uiScrollContainer: 'ui-store-scroll-container',
                uiScrollBar: 'ui-store-scroll-bar',
                uiStud: 'ui-icon-lego-stud',
                uiLogoDC: 'ui-logo-dc',
                uiLogoLego: 'ui-logo-lego',
                uiLogoLegoBatman: 'ui-logo-lego-batman',
                uiRecapBatman: 'ui-recap-batman',
                uiRecapScoreBG: 'ui-recap-score-container',
                uiRecapTrophy: 'ui-recap-high-score-trophy',
                uiSideFrame: 'ui-side-frame',
                uiSplashBG: 'ui-splash-layer-bg',
                uiSplashCar: 'ui-splash-layer-car',
                uiSplashRain1: 'ui-splash-layer-rain-part-1',
                uiSplashRain2: 'ui-splash-layer-rain-part-2',
                uiSplashHiScore: 'ui-store-hi-score-splash',
                pauseBatwing: 'ui-pause-batwing',
                uiStoreBG1: 'ui-store-bg',
                uiStoreBG2: 'ui-store-bg-2',
                uiStoreBuyBtn: 'ui-store-btn-buy',
                uiStoreBuyBtnGrey: 'ui-store-btn-buy-grey',
                uiStoreBuyBtnMax: 'ui-store-max',
                rocketBoosterIcon: 'ui-store-con-icon-booster',
                studIcon: 'ui-store-con-icon-coin',
                healthIcon: 'ui-store-con-icon-health',
                magnetIcon: 'ui-store-con-icon-magnet',
                homingRocketIcon: 'ui-store-con-icon-rocket',
                uiStoreContainer: 'ui-store-container',
                uiStoreHighlight: 'ui-store-highlight',
                uiDialogueBg: 'ui-dialogue-popup',
                uiDialogueButton: 'ui-dialogue-btn',
                uiStoryPage1: 'ui-story-pg-1',
                uiStoryPage2: 'ui-story-pg-2',
                uiStoryPage3: 'ui-story-pg-3',
                uiStoryPage4: 'ui-story-pg-4',
                uiStoryBatcar: 'ui-story-1-batcar',
                uiStoryBatwing: 'ui-story-3-batwing',
                uiStoryBatmobile: 'ui-story-4-batmobile',
                uiStoryCop: 'ui-story-2-police',
                uiStoryFreeze: 'ui-story-4-mrfreeze',
                uiStoryRiddler: 'ui-story-2-riddler',
                uiStoryVillians: 'ui-story-2-villans',
                uiStoryNextBtn: 'ui-story-next-btn',
                uiTransitionBg: 'ui-transition-bg',
                uiTransitionBatman: 'ui-transition-batman'
            });
            this.loadImages('./assets/images/ui/lbm-ui-hud-', {
                uiPauseButton: 'btn-pause',
                hudCoinBG: 'coin-count',
                hudMagnetBG: 'py-count',
                hudBoostBG: 'pb-count',
                hudHealth2: 'health-count-2',
                hudHealth3: 'health-count-3',
                hudHealth4: 'health-count-4',
                hudHealth5: 'health-count-5',
                hudHealthDot: 'health-count-green',
                hudHealthIcon: 'health-icon',
                hudHealthIconHit: 'health-icon-hitreaction',
                hudMagnetTimeBG: 'magnet-time',
                hudScoreBG: 'score-bg',
                hudRedBlur: 'red-text-blur',
                hudMissileCounter: 'rocket-count',
                hudBoostCounter: 'boost-count',
                hudMagnetCounter: 'magnet-count',
                bossHealth2: 'boss-health-2',
                bossHealth3: 'boss-health-3',
                bossHealth4: 'boss-health-4',
                bossHealth5: 'boss-health-5',
                bossHealth6: 'boss-health-6',
                bossHealthDot: 'boss-health-red',
                JokerBossIcon: 'boss-joker',
                JokerWarning: 'warrning-joker',
                FreezeBossIcon: 'boss-freeze',
                FreezeWarning: 'warrning-freeze',
                RiddlerBossIcon: 'boss-riddler',
                RiddlerWarning: 'warrning-riddler'
            });
            this.loadImages('./assets/images/ui/tutorial/lbm-', {
                helpMoveWeb: 'help-screen-arrow-keys-move',
                helpMoveMobile: 'help-screen-drag-to-move',
                helpEnemies: 'help-screen-avoid-enemies',
                helpObstacles: 'help-screen-avoid-obstacles',
                helpBG: 'help-screen-background',
                helpCollect: 'help-screen-collect-blocks-studs',
                helpExit: 'help-screen-exit-icon',
                tutorialMoveWeb: 'tutorial-arrow-keys-move',
                tutorialMoveMobile: 'tutorial-drag-to-move',
                tutorialEnemies: 'tutorial-avoid-enemies',
                tutorialObstacles: 'tutorial-avoid-obstacles',
                tutorialCollect: 'tutorial-collect-blocks-studs',
                tutorialWarningGlow: 'tutorial-warning-glow',
                tutorialWarningIcon: 'tutorial-warning-sign',
            });
            this.loadImages('./locale/' + F84.Game.Instance.locale + '/images/', {
                'uiSplashLogo': 'lbm-ui-logo-splash'
            });
            var chunks = this.cache.getJSON('chunks');
            for (var environmentName in chunks) {
                var environment = chunks[environmentName];
                var easy = environment.easy;
                var medium = environment.medium;
                var hard = environment.hard;
                for (var _i = 0, _a = [easy, medium, hard]; _i < _a.length; _i++) {
                    var difficulty = _a[_i];
                    for (var _b = 0, difficulty_1 = difficulty; _b < difficulty_1.length; _b++) {
                        var name_3 = difficulty_1[_b];
                        this.load.json(name_3, './assets/json/chunks/' + name_3 + '.json');
                    }
                }
            }
            this.load.json('upgrades', './assets/json/upgrades.json');
            this.load.json('environments', './assets/json/environments.json');
            this.load.json('obstaclesTileset', './assets/json/obstacles-tileset.json');
            this.loadAudio('./assets/audio/', {
                menuMusic: 'music-in-a-heartbeat',
                gameMusic: 'music-shiny-tech-ii',
                button: 'sfx-button-positive',
                buttonNegative: 'sfx-button-negative',
                studCollect: 'sfx-stud-collect',
                vehicleTransform: 'sfx-transform',
                powerUpBricks: 'sfx-power-up-bricks',
                upgradePurchased: 'sfx-upgrade',
                impact: 'sfx-impact',
                rocketCollect: 'sfx-rocket-collect',
                magnetCollect: 'sfx-magnet-collect',
                boosterCollect: 'sfx-booster-collect',
                rocketLaunch: 'sfx-rocket-launch',
                jokerBalloon: 'sfx-joker-balloon',
                jokerRocket: 'sfx-joker-rocket',
                jokerGlove: 'sfx-joker-glove',
                riddlerRocket: 'sfx-riddler-rocket',
                riddlerHat: 'sfx-riddler-hat',
                riddlerGiftExplode: 'sfx-riddler-gift-explode',
                freezeBeam: 'sfx-freeze-beam',
                freezeShards: 'sfx-freeze-shards',
                freezeSnowball: 'sfx-freeze-snowball',
                warning: 'sfx-warning',
                streak: 'sfx-streak',
                explosion1: 'sfx-explosion-1',
                explosion2: 'sfx-explosion-2',
                explosion3: 'sfx-explosion-3',
                boostActivate: 'sfx-boost-activate',
                doorClose: 'sfx-door-close',
                swoosh: 'sfx-swoosh',
                storyJoker: 'sfx-story-joker',
                storyRiddler: 'sfx-story-riddler',
                storyPlane: 'sfx-story-plane',
                storyFreeze: 'sfx-story-freeze',
                storySkid: 'sfx-story-skid'
            });
        };
        Preloader.prototype.loadAnimation = function (name, path, img, json) {
            this.load.atlasJSONHash(name, path + img + '.png', path + json + '.json');
        };
        Preloader.prototype.loadAudio = function (prefix, map, suffix) {
            if (suffix === void 0) { suffix = '.ogg'; }
            if (this._audioKeys == null)
                this._audioKeys = [];
            for (var name_4 in map) {
                this.load.audio(name_4, [prefix + map[name_4] + '.mp3']);
                this._audioKeys.push(name_4);
            }
        };
        Preloader.prototype.loadUpdate = function (game) {
            _super.prototype.loadUpdate.call(this, game);
            var totalFiles = this.load.totalQueuedFiles() + this.load.totalLoadedFiles();
            var filesLoaded = this.load.totalLoadedFiles();
            var percentComplete = filesLoaded / totalFiles;
            this._preloader.setProgress(percentComplete);
        };
        Preloader.prototype.resize = function () {
            _super.prototype.resize.call(this);
        };
        Preloader.prototype.create = function () {
            var _this = this;
            new F84.UITransition(this.game, function () {
                _this.game.state.start('MainMenu');
            }, this);
        };
        return Preloader;
    }(F84.ResizableState));
    F84.Preloader = Preloader;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ResizableGroup = (function (_super) {
        __extends(ResizableGroup, _super);
        function ResizableGroup(game, parent, name, addToStage, enableBody, physicsBodyType) {
            var _this = _super.call(this, game, parent, name, addToStage, enableBody, physicsBodyType) || this;
            game.scale.onSizeChange.add(function () {
                _this.resize();
            }, _this);
            return _this;
        }
        ResizableGroup.prototype.resize = function () {
        };
        ResizableGroup.prototype.destroy = function (destroyChildren, soft) {
            if (this.game) {
                this.game.scale.onSizeChange.removeAll(this);
            }
            _super.prototype.destroy.call(this, destroyChildren, soft);
        };
        return ResizableGroup;
    }(Phaser.Group));
    F84.ResizableGroup = ResizableGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PreloaderGroup = (function (_super) {
        __extends(PreloaderGroup, _super);
        function PreloaderGroup(game) {
            var _this = _super.call(this, game) || this;
            var bounds = game.world.bounds;
            var loadBG = game.add.sprite(0, 0, 'loadBG', null, _this);
            F84.SpriteUtils.aspectFill(loadBG, bounds);
            var loadBar = game.add.sprite(0, 0, 'loadBar', null, _this);
            var loadLogo = game.add.sprite(0, 0, 'loadLogo', null, _this);
            loadLogo.scale.set(0.75);
            var loadFill = _this._loadFill = game.add.sprite(0, 0, 'loadFill', null, _this);
            var loadMask = _this._loadMask = game.add.graphics(0, 0, _this);
            loadFill.mask = loadMask;
            var legoLogo = game.add.sprite(0, 0, 'uiLegoBatmanLogo', null, _this);
            var batman80Logo = game.add.sprite(0, 0, 'uiBatman80Logo', null, _this);
            _this._onResize = function () {
                loadBG.alignIn(bounds, Phaser.CENTER);
                loadBar.alignIn(bounds, Phaser.CENTER, -1.5, 94.5);
                loadLogo.alignIn(bounds, Phaser.CENTER, 0, 0);
                loadFill.alignIn(loadBar, Phaser.CENTER, 0, 0);
                legoLogo.alignIn(bounds, Phaser.CENTER, 0, -185.5);
                batman80Logo.alignIn(bounds, Phaser.CENTER, 0, 224.5);
            };
            _this.resize();
            return _this;
        }
        PreloaderGroup.prototype.setProgress = function (percentComplete) {
            this._loadMask.beginFill(0x00000, 1);
            this._loadMask.drawRect(this._loadFill.x, this._loadFill.y, this._loadFill.width * percentComplete, this._loadFill.height);
            this._loadMask.endFill();
            this._loadFill.mask = this._loadMask;
        };
        PreloaderGroup.prototype.resize = function () {
            this._onResize();
        };
        return PreloaderGroup;
    }(F84.ResizableGroup));
    F84.PreloaderGroup = PreloaderGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var RecapScreen = (function (_super) {
        __extends(RecapScreen, _super);
        function RecapScreen() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        RecapScreen.prototype.init = function (data) {
            this.data = data;
        };
        RecapScreen.prototype.create = function () {
            var _this = this;
            this.game.input.onDown.add(function () { return F84.Fullscreen.startFullscreen(_this.game); });
            var batman = this.add.sprite(0, 0, 'uiRecapBatman');
            batman.anchor.set(0.5);
            this.add.tween(batman).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            var sideFrameLeft = this.add.sprite(0, 0, 'uiSideFrame');
            var sideFrameRight = this.add.sprite(0, 0, 'uiSideFrame');
            sideFrameRight.anchor.set(0.5);
            sideFrameRight.angle = 180;
            var header = this.add.group();
            var headerBG = this.add.sprite(0, 0, 'uiHeaderGlow', null, header);
            headerBG.scale.set(1.5);
            headerBG.anchor.set(0.5);
            var style = { fontSize: 64, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 20 };
            var headerText = this.add.text(0, 0, F84.Strings.get('RecapHeader'), style, header);
            headerText.alignIn(headerBG, Phaser.CENTER);
            this.add.tween(header).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            this.add.tween(header.scale).from({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Back.Out, true);
            var scoreGroup = this.add.group();
            var scoreBG = this.add.sprite(0, 0, 'uiRecapScoreBG', null, scoreGroup);
            scoreBG.anchor.set(0.5);
            var scoreStyle = { fontSize: 36, fill: F84.GameColors.ORANGE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 15 };
            var scoreHeader = this.add.text(0, 0, F84.Strings.get('RecapScore'), scoreStyle, scoreGroup);
            scoreHeader.alignTo(scoreBG, Phaser.TOP_CENTER, 0, -50);
            var score = this.add.text(0, 0, '' + this.data.score, style, scoreGroup);
            score.alignIn(scoreBG, Phaser.CENTER, 0, 2);
            var studsBG = this.add.sprite(0, 110, 'uiRecapScoreBG', null, scoreGroup);
            studsBG.anchor.set(0.5);
            var studIcon = this.add.sprite(0, 0, 'uiStud', null, scoreGroup);
            studIcon.alignIn(studsBG, Phaser.LEFT_CENTER, -120, 0);
            style.fontSize = 56;
            var studs = this.add.text(0, 0, '' + this.data.studsEarned, style, scoreGroup);
            studs.alignTo(studIcon, Phaser.RIGHT_CENTER, 0, 2);
            this.add.tween(scoreGroup).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true, 250);
            this.add.tween(scoreGroup.scale).from({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Back.Out, true, 250);
            var values = { studs: 0, score: 0 };
            var scoreTween = this.add.tween(values).to({ studs: this.data.studsEarned, score: this.data.score }, 1000, Phaser.Easing.Quadratic.Out, true, 500);
            studs.update = function () {
                studs.text = "" + Math.round(values.studs);
                score.text = "" + Math.round(values.score);
            };
            var countTime = 0;
            scoreTween.onUpdateCallback(function () {
                countTime -= _this.game.time.physicsElapsed;
                if (countTime <= 0) {
                    _this.game.sound.play('studCollect');
                    countTime = 0.05;
                }
            });
            scoreTween.onComplete.add(function () {
                _this.game.sound.play('streak');
            });
            var highScoreGroup;
            if (this.data.highScore) {
                highScoreGroup = this.add.group(scoreGroup);
                var highScoreTrophy = this.add.sprite(0, 0, 'uiRecapTrophy', null, highScoreGroup);
                highScoreTrophy.anchor.set(0.5);
                var style_1 = { fontSize: 36, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, wordWrap: true, stroke: '#00000000', strokeThickness: 10 };
                var highScoreText = this.add.text(0, 0, F84.Strings.get('RecapHighScore'), style_1, highScoreGroup);
                highScoreText.alignTo(highScoreTrophy, Phaser.BOTTOM_CENTER, 0, 0);
                highScoreGroup.alignTo(scoreBG, Phaser.RIGHT_CENTER, -70, 40);
                var tween = this.add.tween(highScoreGroup.scale).from({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Back.Out, false, 800);
                tween.start();
                this.add.tween(highScoreGroup).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true, 800);
            }
            var shopPrompt;
            var hasUpgrade = false;
            var saveData = F84.PlayerData.Instance.saveData;
            for (var upgrade in saveData.upgrades) {
                if (saveData.upgrades[upgrade] > 0)
                    hasUpgrade = true;
            }
            var showShopPrompt = (!hasUpgrade && saveData.studs >= 250);
            if (showShopPrompt) {
                var overlay = F84.Overlay.create(this.game, 0.5);
                var alpha = overlay.alpha;
                overlay.alpha = 0;
                var overlayTween_1 = this.add.tween(overlay).to({ alpha: alpha }, 250, Phaser.Easing.Linear.None);
                scoreTween.onComplete.add(function () { return overlayTween_1.start(); });
                shopPrompt = this.add.group();
                var glow = this.add.sprite(0, 0, 'uiHeaderGlow', null, shopPrompt);
                glow.anchor.set(0.5);
                style = { fontSize: 36, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, wordWrap: true, wordWrapWidth: 380, align: 'center', stroke: '#00000000', strokeThickness: 10 };
                var text = this.add.text(0, 0, F84.Strings.get('ShopPrompt'), style, shopPrompt);
                text.lineSpacing = -15;
                text.alignIn(glow, Phaser.CENTER);
                var pulseTween = this.add.tween(glow);
                pulseTween.to({ alpha: 0.6 }, 800, Phaser.Easing.Linear.None, true, 0, -1, true);
                shopPrompt.alpha = 0;
                var promptTween_1 = this.add.tween(shopPrompt.scale).from({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Back.Out);
                var promptFade_1 = this.add.tween(shopPrompt).to({ alpha: 1 }, 250, Phaser.Easing.Linear.None);
                scoreTween.onComplete.add(function () { promptTween_1.start(); promptFade_1.start(); });
            }
            var buttons = this.add.group();
            var buttonGlow = this.add.sprite(0, 0, 'uiHeaderGlow', null, buttons);
            buttonGlow.scale.set(1.8, 0.8);
            buttonGlow.anchor.set(0.5);
            var playButton = this.add.button(0, 0, 'uiReplayButton', this.replay);
            buttons.add(playButton);
            var homeButton = this.add.button(0, 0, 'uiHomeButton', this.home, this);
            buttons.add(homeButton);
            var upgradeButton = this.add.button(0, 0, 'uiShopButton', this.upgrades, this);
            buttons.add(upgradeButton);
            homeButton.alignIn(buttonGlow, Phaser.CENTER, -250 + (76 / 2), 30);
            upgradeButton.alignIn(buttonGlow, Phaser.CENTER, 0 + (76 / 2), 30);
            playButton.alignIn(buttonGlow, Phaser.CENTER, 250 + (76 / 2), 30);
            if (showShopPrompt) {
                playButton.destroy();
                homeButton.destroy();
            }
            buttons.alpha = 0;
            var buttonsTween = this.add.tween(buttons.scale).from({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Back.Out);
            var buttonsFade = this.add.tween(buttons).to({ alpha: 1 }, 250, Phaser.Easing.Linear.None);
            scoreTween.onComplete.add(function () { buttonsTween.start(); buttonsFade.start(); });
            this._onResize = function () {
                batman.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 0, 0);
                header.alignIn(_this.world.bounds, Phaser.TOP_CENTER, 0, -90);
                scoreGroup.alignIn(_this.world.bounds, Phaser.CENTER, 0, -180 - 55);
                buttons.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 0, 30);
                if (shopPrompt)
                    shopPrompt.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 0, -200);
                var scale = 1;
                var bounds = _this.world.bounds;
                if (bounds.height > sideFrameLeft.height) {
                    scale = F84.SpriteUtils.getAspectFill(sideFrameLeft, bounds);
                }
                sideFrameLeft.scale.set(scale);
                sideFrameLeft.alignTo(bounds, Phaser.LEFT_CENTER, -bounds.width * 0.18 * scale, 0);
                sideFrameRight.scale.set(scale);
                sideFrameRight.alignTo(bounds, Phaser.RIGHT_CENTER, -bounds.width * 0.18 * scale, 0);
            };
            this.resize();
            F84.Music.switchMusic(this.game, 'menuMusic');
        };
        RecapScreen.prototype.resize = function () {
            this._onResize();
        };
        RecapScreen.prototype.home = function () {
            var _this = this;
            this.game.sound.play('button');
            new F84.UITransition(this.game, function () { return _this.game.state.start('MainMenu'); });
        };
        RecapScreen.prototype.replay = function () {
            var _this = this;
            this.game.sound.play('button');
            new F84.UITransition(this.game, function () { return _this.game.state.start('GameState'); });
        };
        RecapScreen.prototype.upgrades = function () {
            var _this = this;
            this.game.sound.play('button');
            new F84.UITransition(this.game, function () { return _this.game.state.start('ShopState'); });
        };
        return RecapScreen;
    }(F84.ResizableState));
    F84.RecapScreen = RecapScreen;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var RotateDeviceGroup = (function (_super) {
        __extends(RotateDeviceGroup, _super);
        function RotateDeviceGroup(game) {
            var _this = _super.call(this, game, game.stage.stage) || this;
            _this.game.input.enabled = false;
            var graphics = _this.game.add.graphics(0, 0, _this);
            graphics.beginFill(0x000000, 1);
            graphics.drawRect(0, 0, F84.Game.Instance.width, F84.Game.Instance.height);
            graphics.endFill();
            var rotateImage = _this.game.add.image(0, 0, 'rotateDevice', null, _this);
            var scale = F84.Game.Instance.width / rotateImage.width;
            rotateImage.width *= scale;
            rotateImage.height *= scale;
            rotateImage.alignIn(_this.game.world.bounds, Phaser.CENTER);
            _this.game.scale.onSizeChange.add(function () {
                if (F84.Game.Instance.height > F84.Game.Instance.width) {
                    _this.destroy(true, false);
                }
            }, _this);
            return _this;
        }
        RotateDeviceGroup.prototype.update = function () {
            _super.prototype.update.call(this);
            this.game.world.bringToTop(this);
            if (F84.Game.Instance.height > F84.Game.Instance.width) {
                this.destroy(true, false);
            }
        };
        RotateDeviceGroup.prototype.destroy = function (destroyChildren, soft) {
            if (this.game) {
                this.game.input.enabled = true;
                this.game.scale.onSizeChange.removeAll(this);
            }
            _super.prototype.destroy.call(this, destroyChildren, soft);
        };
        return RotateDeviceGroup;
    }(Phaser.Group));
    F84.RotateDeviceGroup = RotateDeviceGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ScrollBar = (function (_super) {
        __extends(ScrollBar, _super);
        function ScrollBar(game, barKey, containerKey, dragGroup) {
            var _this = _super.call(this, game) || this;
            _this._dragGroup = dragGroup;
            _this._container = new F84.NineSlicedSprite(_this.game, 0, 0, containerKey, 12, 163, 10, 3, 10, 3);
            _this._container.anchor.set(0.5, 0);
            _this.game.add.existing(_this._container);
            _this.add(_this._container);
            _this._bar = _this.game.add.image(0, 0, barKey, null, _this);
            _this._bar.anchor.set(0.5, 0);
            return _this;
        }
        ScrollBar.prototype.setScrollHeight = function (value) {
            this._scrollHeight = value;
            this.update();
        };
        ScrollBar.prototype.update = function () {
            this._container.resize(12, this._scrollHeight);
            var pos = 1 - (this._dragGroup.y - this._dragGroup.minY) / (this._dragGroup.maxY - this._dragGroup.minY);
            pos = Phaser.Math.clamp(pos, 0, 1);
            this._bar.y = (this._scrollHeight - this._bar.height) * pos;
        };
        return ScrollBar;
    }(Phaser.Group));
    F84.ScrollBar = ScrollBar;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var SettingsGroup = (function (_super) {
        __extends(SettingsGroup, _super);
        function SettingsGroup(game, onReset, onResetContext) {
            var _this = _super.call(this, game) || this;
            _this.onReset = onReset;
            _this.onResetContext = onResetContext;
            var _overlay = _this.game.add.graphics(0, 0, _this);
            _overlay.inputEnabled = true;
            _overlay.beginFill(0x000000, 0.7);
            _overlay.drawRect(0, 0, F84.Game.Instance.width, F84.Game.Instance.height);
            _overlay.endFill();
            var _background = _this.game.add.image(0, 0, 'uiDialogueBg', null, _this);
            var style = { fill: '#f5e501', fontSize: 48, font: F84.Fonts.BoldFont, wordWrap: true, wordWrapWidth: 500, align: "center", stroke: '#00000000', strokeThickness: 10 };
            var _message = _this.game.add.text(0, 0, F84.Strings.get('Settings'), style, _this);
            var _resetButton = _this.game.add.button(0, 0, 'uiRestartButton', _this.reset, _this, null, null, null, null, _this);
            if (!F84.PlayerData.Instance.saveData.gameStarted) {
                _resetButton.inputEnabled = false;
                _resetButton.loadTexture('uiRestartInactiveButton');
            }
            var _soundButton = F84.UIFactory.createLargeSoundButton(_this.game, _this);
            _soundButton.scale.set(0.72);
            var _closeButton = _this.game.add.button(0, 0, 'uiDialogueButton', function () { return _this.destroy(); }, _this, null, null, null, null, _this);
            style.fontSize = 28;
            style.fill = 'white';
            var _resetText = _this.game.add.text(0, 0, '', style, _this);
            _resetText.setText(F84.Strings.get('ResetYes'));
            var _soundText = _this.game.add.text(0, 0, F84.Strings.get('Sound'), style, _this);
            var _closeText = _this.game.add.text(0, 0, F84.Strings.get('Close'), style, _this);
            _this._resize = function () {
                _background.alignIn(_this.game.world.bounds, Phaser.CENTER);
                _message.alignIn(_background, Phaser.CENTER, 0, -120);
                _resetButton.alignIn(_background, Phaser.CENTER, -72.25, 32.25);
                _soundButton.alignIn(_background, Phaser.CENTER, 115, 33);
                _closeButton.alignIn(_background, Phaser.BOTTOM_CENTER, 0, _closeButton.height / 3);
                _resetText.alignIn(_resetButton, Phaser.BOTTOM_CENTER, -36);
                _soundText.alignIn(_soundButton, Phaser.BOTTOM_CENTER, -36);
                _closeText.alignIn(_closeButton, Phaser.CENTER, -9, -6);
            };
            _this.resize();
            return _this;
        }
        SettingsGroup.prototype.reset = function () {
            this.onReset.call(this.onResetContext);
            this.destroy();
        };
        SettingsGroup.prototype.resize = function () {
            this._resize();
        };
        return SettingsGroup;
    }(F84.ResizableGroup));
    F84.SettingsGroup = SettingsGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ShopState = (function (_super) {
        __extends(ShopState, _super);
        function ShopState() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        ShopState.prototype.create = function () {
            var _this = this;
            var bounds = F84.UIUtils.getMobileBounds(this.world.bounds);
            ShopState._numShopVisits++;
            var bgKey = ShopState._numShopVisits % 2 == 0 ? 'uiStoreBG1' : 'uiStoreBG2';
            var background = this.add.sprite(0, 0, bgKey);
            F84.SpriteUtils.aspectFill(background, this.world.bounds);
            var dragGroup = new F84.DraggableGroup(this.game, false, true);
            dragGroup.y = -175;
            dragGroup.maxY = -175;
            dragGroup.minY = -1800 + this.world.bounds.height;
            dragGroup.softBounds = true;
            var dragBG = this.add.sprite(0, 0, 'whiteSquare', null, dragGroup);
            dragGroup.enableDragging(dragBG);
            dragBG.width = this.world.bounds.width;
            dragBG.alpha = 0;
            dragBG.height = 10000;
            this.upgrades = [];
            for (var key in F84.PlayerData.Instance.saveData.upgrades) {
                var group = new F84.UpgradeGroup(this.game, key, dragGroup);
                group.onPurchase.add(this.onPurchase, this);
                this.upgrades.push(group);
                dragGroup.add(group);
            }
            var gradient = this.add.image(0, 0, 'uiStoreGradient');
            var scrollBar = new F84.ScrollBar(this.game, 'uiScrollBar', 'uiScrollContainer', dragGroup);
            var backButton = this.add.button(0, 0, 'uiBackButton', this.back, this);
            var titleBG = this.add.sprite(0, 0, 'uiHeaderGlow');
            var title = this.add.text(0, 0, F84.Strings.get('ShopTitle'), { fontSize: 56, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 });
            var currencyGroup = F84.UIFactory.createCurrencyGroup(this.game);
            this.resize = function () {
                _super.prototype.resize.call(_this);
                bounds = F84.UIUtils.getMobileBounds(_this.world.bounds);
                F84.SpriteUtils.aspectFill(background, _this.world.bounds);
                background.alignIn(_this.world.bounds, Phaser.CENTER);
                gradient.width = F84.Game.Instance.width;
                gradient.alignIn(_this.world.bounds, Phaser.TOP_CENTER);
                titleBG.alignIn(_this.world.bounds, Phaser.TOP_CENTER, 0, 30);
                title.alignIn(titleBG, Phaser.CENTER);
                backButton.alignIn(bounds, Phaser.TOP_LEFT, -25, -25);
                currencyGroup.alignIn(bounds, Phaser.TOP_RIGHT, -20, -30);
                dragGroup.align(1, -1, _this.world.bounds.width, 296, Phaser.TOP_CENTER);
                scrollBar.setScrollHeight(F84.Game.Instance.height - gradient.height - 60);
                scrollBar.alignIn(bounds, Phaser.CENTER, 350, 60);
            };
            this.resize();
        };
        ShopState.prototype.back = function () {
            var _this = this;
            this.game.sound.play('buttonNegative');
            new F84.UITransition(this.game, function () { return _this.game.state.start('MainMenu'); });
        };
        ShopState.prototype.onPurchase = function () {
            this.upgrades.forEach(function (upgrade) {
                upgrade.checkAffordability();
            });
        };
        ShopState._numShopVisits = 0;
        return ShopState;
    }(F84.ResizableState));
    F84.ShopState = ShopState;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var StoryIntro = (function (_super) {
        __extends(StoryIntro, _super);
        function StoryIntro() {
            var _this = _super !== null && _super.apply(this, arguments) || this;
            _this.numPages = 4;
            return _this;
        }
        StoryIntro.prototype.create = function () {
            var _this = this;
            this.game.input.onDown.add(function () { return F84.Fullscreen.startFullscreen(_this.game); });
            this.pageNumber = 1;
            var style = { fill: 'white', fontSize: 26, wordWrap: true, wordWrapWidth: 700, align: 'center', font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var background = this.background = this.add.sprite(0, 0, 'uiStoryPage1');
            this.midGroup = this.add.group();
            var playButton = this.add.button(0, 0, 'uiStoryNextBtn', this.next, this);
            var nextText = this.add.text(0, 0, F84.Strings.get('next'), style);
            var text = this.text = this.add.text(0, 0, F84.Strings.get("StoryPage1"), style);
            this._onResize = function () {
                F84.SpriteUtils.aspectFill(background, _this.world.bounds);
                background.alignIn(_this.world.bounds, Phaser.CENTER);
                playButton.alignIn(_this.world.bounds, Phaser.BOTTOM_CENTER, 4.5, -29);
                nextText.alignIn(playButton, Phaser.CENTER);
                text.alignTo(playButton, Phaser.TOP_CENTER, 0, 75);
            };
            this.resize();
            this.onSwitchPage = this.page1();
        };
        StoryIntro.prototype.resize = function () {
            this._onResize();
        };
        StoryIntro.prototype.next = function () {
            var _this = this;
            this.game.sound.play('button');
            this.pageNumber++;
            if (this.pageNumber <= this.numPages) {
                this.background.loadTexture('uiStoryPage' + this.pageNumber);
                this.text.text = F84.Strings.get('StoryPage' + this.pageNumber);
                if (this.onSwitchPage)
                    this.onSwitchPage();
                this.onSwitchPage = [null, null, this.page2, this.page3, this.page4][this.pageNumber].call(this);
            }
            else {
                new F84.UITransition(this.game, function () { return _this.game.state.start('GameState'); });
            }
        };
        StoryIntro.prototype.page1 = function () {
            var _this = this;
            var batmobile = this.add.sprite(0, 0, 'uiStoryBatcar', null, this.midGroup);
            batmobile.alignIn(this.world.bounds, Phaser.CENTER, 0, -130);
            this.add.tween(batmobile).from({ x: batmobile.x - 600, y: batmobile.y - 400 }, 500, Phaser.Easing.Quadratic.Out, true, 600);
            this.add.tween(batmobile).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true, 600);
            this.add.tween(batmobile.anchor).to({ y: 0.004 }, 60, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            setTimeout(function () {
                _this.game.sound.play('boostActivate');
            }, 350);
            return function () { return batmobile.destroy(); };
        };
        StoryIntro.prototype.page2 = function () {
            var villains = this.add.sprite(0, 0, 'uiStoryVillians', null, this.midGroup);
            villains.alignIn(this.world.bounds, Phaser.CENTER, -260, -210);
            this.add.tween(villains).from({ x: villains.x + 600, y: villains.y - 200 }, 500, Phaser.Easing.Quadratic.Out, true);
            this.add.tween(villains).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            this.add.tween(villains.anchor).to({ y: 0.003 }, 60, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            var riddler = this.add.sprite(0, 0, 'uiStoryRiddler', null, this.midGroup);
            riddler.anchor.set(0.5);
            riddler.alignIn(this.world.bounds, Phaser.CENTER, 20, -540);
            riddler.angle = -5;
            this.add.tween(riddler).from({ x: riddler.x + 400, y: riddler.y - 800 }, 700, Phaser.Easing.Quadratic.Out, true);
            this.add.tween(riddler).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            this.add.tween(riddler).to({ angle: 5 }, 1300, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            this.add.tween(riddler.anchor).to({ y: 0.3 }, 1500, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            var riddlerSfx = this.game.sound.play('storyRiddler', 0.5, true);
            return function () {
                riddlerSfx.stop();
                villains.destroy();
                riddler.destroy();
            };
        };
        StoryIntro.prototype.page3 = function () {
            var batmobile = this.add.sprite(0, 0, 'uiStoryBatwing', null, this.midGroup);
            batmobile.anchor.set(0.5);
            batmobile.alignIn(this.world.bounds, Phaser.CENTER, -50, -120);
            this.add.tween(batmobile).from({ x: batmobile.x - 300, y: batmobile.y - 300 }, 500, Phaser.Easing.Quadratic.Out, true);
            this.add.tween(batmobile).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            this.add.tween(batmobile).to({ angle: 1.5 }, 800, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            this.add.tween(batmobile.anchor).to({ y: 0.55 }, 1500, Phaser.Easing.Sinusoidal.InOut, true, 0, -1, true);
            var planeSfx = this.game.sound.play('storyPlane', 0.25, true);
            return function () {
                planeSfx.stop();
                batmobile.destroy();
            };
        };
        StoryIntro.prototype.page4 = function () {
            var _this = this;
            this.midGroup.add(this.background);
            this.background.anchor.set(0.5, 0.4);
            this.midGroup.alignIn(this.world.bounds, Phaser.CENTER);
            var batmobile = this.add.sprite(0, 0, 'uiStoryBatmobile', null, this.midGroup);
            batmobile.anchor.set(0.5);
            batmobile.alignIn(this.background, Phaser.CENTER, -250, -130);
            this.add.tween(batmobile).from({ x: batmobile.x - 500, y: batmobile.y - 300 }, 500, Phaser.Easing.Quadratic.Out, true, 300);
            this.add.tween(batmobile).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true, 300);
            var freeze = this.add.sprite(0, 0, 'uiStoryFreeze', null, this.midGroup);
            freeze.anchor.set(0.5);
            freeze.alignIn(this.background, Phaser.CENTER, 250, -40);
            this.add.tween(freeze).from({ x: freeze.x + 300, y: freeze.y - 100 }, 500, Phaser.Easing.Quadratic.Out, true);
            this.add.tween(freeze).from({ alpha: 0 }, 250, Phaser.Easing.Linear.None, true);
            this.add.tween(this.midGroup.scale).to({ x: 1.2, y: 1.2 }, 10000, Phaser.Easing.Quadratic.InOut, true);
            this.game.sound.play('storyFreeze');
            setTimeout(function () {
                _this.game.sound.play('storySkid', 0.5);
            }, 250);
        };
        return StoryIntro;
    }(F84.ResizableState));
    F84.StoryIntro = StoryIntro;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Strings = (function () {
        function Strings() {
        }
        Strings.get = function (id) {
            if (this._json == null) {
                this._json = F84.Game.Instance.cache.getJSON("strings");
            }
            if (this._json[id] === undefined) {
                return 'MISSING';
            }
            return this._json[id];
        };
        Strings._json = null;
        return Strings;
    }());
    F84.Strings = Strings;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var UIFactory = (function () {
        function UIFactory() {
        }
        UIFactory.createLargeSoundButton = function (game, group) {
            return this.createSoundButton(game, 'uiSoundButton', 'uiSoundButtonOff', group);
        };
        UIFactory.createSmallSoundButton = function (game, group) {
            return this.createSoundButton(game, 'uiSoundButtonSmall', 'uiSoundButtonSmallOff', group);
        };
        UIFactory.createSoundButton = function (game, on, off, group) {
            var start = game.sound.mute ? off : on;
            var button = game.add.button(0, 0, start, function () {
                game.sound.mute = !game.sound.mute;
                button.loadTexture(game.sound.mute ? off : on);
            }, this, null, null, null, null, group);
            return button;
        };
        UIFactory.createCurrencyGroup = function (game) {
            var group = game.add.group();
            var image = game.add.image(0, 0, 'hudCoinBG', null, group);
            image.anchor.set(0.5, 0.5);
            var text = game.add.text(0, 0, '', { fontSize: 30, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 }, group);
            text.anchor.set(0, 0.5);
            text.alignIn(image, Phaser.LEFT_CENTER, -50, 2);
            text.update = function () {
                text.text = String(F84.PlayerData.Instance.saveData.studs);
            };
            text.update();
            return group;
        };
        UIFactory.createDialogueTextButton = function (game, parent, x, y, text, callback, callbackContext) {
            var button = game.add.button(x, y, 'uiDialogueButton', callback, callbackContext, null, null, null, null, parent);
            var textField = game.add.text(0, 0, '', { fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, fontSize: 32, stroke: '#00000000', strokeThickness: 10 }, parent);
            textField.setText(text);
            textField.alignIn(button, Phaser.CENTER, -13, -6);
            button.addChild(textField);
            return button;
        };
        return UIFactory;
    }());
    F84.UIFactory = UIFactory;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var UITransition = (function (_super) {
        __extends(UITransition, _super);
        function UITransition(game, onMidpointCallback, onMidpointContext) {
            var _this = _super.call(this, game, null, null, true) || this;
            if (UITransition.instance != null) {
                _this.destroy(true, false);
            }
            else {
                UITransition.instance = _this;
                var leftBg = game.add.sprite(0, 0, 'uiTransitionBg', null, _this);
                var scale = game.height / leftBg.height;
                leftBg.scale.set(scale);
                leftBg.alignTo(game.world.bounds, Phaser.LEFT_CENTER);
                var rightBg = game.add.sprite(0, 0, 'uiTransitionBg', null, _this);
                rightBg.scale.set(-scale, scale);
                rightBg.alignTo(game.world.bounds, Phaser.RIGHT_CENTER, -rightBg.width);
                var batman = game.add.sprite(0, 0, 'uiTransitionBatman', null, _this);
                batman.alignTo(game.world.bounds, Phaser.RIGHT_CENTER, 0, -15);
                var halfWorld = game.world.bounds.width / 2;
                var duration = 175;
                var easing = Phaser.Easing.Quadratic.Out;
                var closeLTween = _this.game.add.tween(leftBg).to({ x: halfWorld - leftBg.width }, duration, easing, true);
                var closeRTween = _this.game.add.tween(rightBg).to({ x: halfWorld - rightBg.width }, duration, easing, true);
                var batInTween_1 = _this.game.add.tween(batman).to({ x: halfWorld - batman.width / 2 + 108 }, duration, easing, false);
                var openLTween_1 = _this.game.add.tween(leftBg).to({ x: leftBg.x }, duration, easing, false);
                var openRTween_1 = _this.game.add.tween(rightBg).to({ x: rightBg.x }, duration, easing, false);
                var batOutTween_1 = _this.game.add.tween(batman).to({ x: -batman.width }, duration, easing, false);
                setTimeout(function () {
                    _this.game.sound.play('swoosh');
                }, duration / 1.05);
                var playSecond = function () {
                    onMidpointCallback();
                    setTimeout(function () {
                        batOutTween_1.start();
                        _this.game.sound.play('swoosh');
                    }, 250);
                };
                batInTween_1.onComplete.addOnce(playSecond);
                batOutTween_1.onComplete.addOnce(function () {
                    openLTween_1.start();
                    openRTween_1.start();
                    _this.game.sound.play('doorClose');
                    _this.callFunctionWhenTweensComplete(function () {
                        _this.game.stage.removeChild(_this);
                        _this.game.input.enabled = true;
                        _this.destroy(true, false);
                        UITransition.instance = null;
                    }, openLTween_1, openRTween_1);
                });
                _this.callFunctionWhenTweensComplete(function () { return batInTween_1.start(); }, closeLTween, closeRTween);
                _this.game.sound.play('doorClose');
                _this.game.input.enabled = false;
            }
            return _this;
        }
        UITransition.prototype.callFunctionWhenTweensComplete = function (onComplete) {
            var tweens = [];
            for (var _i = 1; _i < arguments.length; _i++) {
                tweens[_i - 1] = arguments[_i];
            }
            var count = tweens.length;
            var numFinished = 0;
            tweens.forEach(function (tween) {
                tween.onComplete.addOnce(function () {
                    numFinished++;
                    if (numFinished >= count)
                        onComplete();
                });
            });
        };
        UITransition.prototype.update = function () {
            this.game.world.bringToTop(this);
        };
        return UITransition;
    }(Phaser.Group));
    F84.UITransition = UITransition;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var UpgradeGroup = (function (_super) {
        __extends(UpgradeGroup, _super);
        function UpgradeGroup(game, upgradeKey, dragGroup) {
            var _this = _super.call(this, game) || this;
            _this._upgradeKey = upgradeKey;
            var background = _this.game.add.sprite(0, 0, 'uiStoreContainer', null, _this);
            var title = _this.game.add.text(0, 0, '', { fontSize: 32, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 }, _this);
            title.setText(F84.Strings.get(upgradeKey + "Name"));
            var description = _this.game.add.text(0, 0, F84.Strings.get(upgradeKey + "Description"), { fontSize: 24, fill: F84.GameColors.WHITE, wordWrapWidth: 420, wordWrap: true, font: F84.Fonts.DefaultFont }, _this);
            var icon = _this.game.add.sprite(0, 0, upgradeKey + 'Icon', null, _this);
            var purchaseButton = _this.game.add.button(0, 0, 'uiStoreBuyBtn');
            _this.add(purchaseButton);
            dragGroup.enableDragging(purchaseButton);
            purchaseButton.onInputUp.add(function () {
                if (dragGroup.dragDistance < 40)
                    _this.purchase();
            });
            var purchaseText = _this.game.add.text(0, 0, '1000', { fontSize: 32, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 });
            purchaseText.alignIn(purchaseButton, Phaser.LEFT_CENTER, -75, 3);
            purchaseButton.addChild(purchaseText);
            _this._upgradeCost = purchaseText;
            _this._upgradeButton = purchaseButton;
            _this.onPurchase = new Phaser.Signal();
            _this._upgradeLevelGroup = _this.game.add.group(_this);
            for (var i = 0; i < 3; i++) {
                var image = _this.game.add.image(0, 0, 'uiStoreHighlight', null, _this._upgradeLevelGroup);
            }
            _this._upgradeLevelGroup.align(-1, 1, 79, 72, Phaser.LEFT_BOTTOM);
            _this.resize = function () {
                _super.prototype.resize.call(_this);
                title.alignIn(background, Phaser.TOP_CENTER, 10, -11);
                description.alignIn(background, Phaser.CENTER, 35, -25);
                icon.alignIn(background, Phaser.TOP_LEFT, -5, -5);
                purchaseButton.alignIn(background, Phaser.BOTTOM_RIGHT, -59, -62);
                _this._upgradeLevelGroup.alignIn(background, Phaser.BOTTOM_LEFT, -86, -58);
            };
            _this.resize();
            _this.updateDisplay();
            return _this;
        }
        UpgradeGroup.prototype.purchase = function () {
            var upgradeCost = this.getCurrentUpgradeCost();
            if (F84.PlayerData.Instance.saveData.studs >= upgradeCost) {
                F84.PlayerData.Instance.saveData.studs -= upgradeCost;
                F84.PlayerData.Instance.saveData.upgrades[this._upgradeKey] += 1;
                F84.PlayerData.Instance.save();
                this.updateDisplay();
                this.onPurchase.dispatch();
                this.game.sound.play('upgradePurchased');
            }
            else {
                this.game.sound.play('buttonNegative');
            }
        };
        UpgradeGroup.prototype.getCurrentUpgradeCost = function () {
            var upgradeIndex = F84.PlayerData.Instance.saveData.upgrades[this._upgradeKey];
            var costArray = F84.GameConfig.get("upgradeCosts");
            if (upgradeIndex >= costArray.length) {
                return -1;
            }
            return costArray[upgradeIndex];
        };
        UpgradeGroup.prototype.updateDisplay = function () {
            var upgradeCost = this.getCurrentUpgradeCost();
            var upgradeIndex = F84.PlayerData.Instance.saveData.upgrades[this._upgradeKey];
            if (upgradeCost == -1) {
                this._upgradeButton.loadTexture('uiStoreBuyBtnMax');
                this._upgradeButton.onInputUp.removeAll();
                this._upgradeCost.text = F84.Strings.get('Max');
                this._upgradeCost.alignIn(this._upgradeButton, Phaser.CENTER, -this._upgradeButton.x, -this._upgradeButton.y);
            }
            else {
                if (upgradeCost > F84.PlayerData.Instance.saveData.studs) {
                    this._upgradeButton.loadTexture('uiStoreBuyBtnGrey');
                }
                this._upgradeCost.text = String(upgradeCost);
            }
            for (var i = 0; i < this._upgradeLevelGroup.children.length; i++) {
                this._upgradeLevelGroup.children[i].alpha = upgradeIndex > i ? 1 : 0;
            }
        };
        UpgradeGroup.prototype.checkAffordability = function () {
            if (this.getCurrentUpgradeCost() > F84.PlayerData.Instance.saveData.studs) {
                this._upgradeButton.loadTexture('uiStoreBuyBtnGrey');
            }
        };
        return UpgradeGroup;
    }(F84.ResizableGroup));
    F84.UpgradeGroup = UpgradeGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var YesNoDialogueGroup = (function (_super) {
        __extends(YesNoDialogueGroup, _super);
        function YesNoDialogueGroup(game, config) {
            var _this = _super.call(this, game) || this;
            _this._onYes = config.yesFunction.bind(config.yesContext);
            if (config.noFunction)
                _this._onNo = config.noFunction.bind(config.noContext);
            _this._overlay = _this.game.add.graphics(0, 0, _this);
            _this._overlay.inputEnabled = true;
            _this._overlay.beginFill(0x000000, 0.7);
            _this._overlay.drawRect(0, 0, F84.Game.Instance.width, F84.Game.Instance.height);
            _this._overlay.endFill();
            _this._dialogueGroup = new Phaser.Group(game, _this);
            _this._background = _this.game.add.image(0, 0, 'uiDialogueBg', null, _this._dialogueGroup);
            _this._message = _this.game.add.text(0, 0, config.messageText, { fill: 'white', fontSize: 34, font: F84.Fonts.BoldFont, wordWrap: true, wordWrapWidth: 500, align: "center" }, _this._dialogueGroup);
            _this._yesButton = F84.UIFactory.createDialogueTextButton(_this.game, _this._dialogueGroup, 0, 0, config.yesButtonText, _this.yesButtonPressed, _this);
            _this._noButton = F84.UIFactory.createDialogueTextButton(_this.game, _this._dialogueGroup, 0, 0, config.noButtonText, _this.noButtonPressed, _this);
            _this.resize();
            game.add.tween(_this._overlay).from({ alpha: 0 }, 150, Phaser.Easing.Quadratic.Out, true);
            game.add.tween(_this._dialogueGroup).from({ y: _this._dialogueGroup.y + 200 }, 300, Phaser.Easing.Quadratic.Out, true, 75);
            game.add.tween(_this._dialogueGroup).from({ alpha: 0 }, 150, Phaser.Easing.Linear.None, true, 75);
            _this.fixedToCamera = config.fixedToCamera;
            return _this;
        }
        YesNoDialogueGroup.prototype.resize = function () {
            var bounds = F84.Game.Instance.getBounds();
            F84.SpriteUtils.aspectFill(this._overlay, bounds);
            this._dialogueGroup.alignIn(bounds, Phaser.CENTER);
            this._message.alignIn(this._background, Phaser.TOP_CENTER, 0, -100);
            this._yesButton.alignIn(this._background, Phaser.BOTTOM_CENTER, -111, -94);
            this._noButton.alignIn(this._background, Phaser.BOTTOM_CENTER, 111, -94);
        };
        YesNoDialogueGroup.prototype.noButtonPressed = function () {
            this.game.sound.play('button');
            if (this._onNo) {
                this._onNo();
            }
            this.destroy();
        };
        YesNoDialogueGroup.prototype.yesButtonPressed = function () {
            this.game.sound.play('button');
            if (this._onYes) {
                this._onYes();
            }
        };
        return YesNoDialogueGroup;
    }(F84.ResizableGroup));
    F84.YesNoDialogueGroup = YesNoDialogueGroup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Enemy = (function (_super) {
        __extends(Enemy, _super);
        function Enemy(game, x, y, key) {
            var _this = this;
            var type = F84.GameState.Instance.upcomingBoss;
            _this = _super.call(this, game, x, y, key + type) || this;
            _this.anchor.set(0.5);
            game.physics.enable(_this);
            _this.activated = false;
            return _this;
        }
        Enemy.prototype.update = function () {
            if (this.body.y + this.height >= F84.GameState.Instance.camController.bounds.top && !this.activated)
                this.activate();
        };
        Enemy.prototype.activate = function () {
            this.activated = true;
        };
        Object.defineProperty(Enemy.prototype, "player", {
            get: function () {
                return F84.GameState.Instance.player;
            },
            enumerable: true,
            configurable: true
        });
        Enemy.prototype.angleToPlayer = function () {
            return this.angleTo(this.player.position);
        };
        Enemy.prototype.angleTo = function (target) {
            var dx = target.x - this.x;
            var dy = target.y - this.y;
            return Math.atan2(dy, dx);
        };
        return Enemy;
    }(Phaser.Sprite));
    F84.Enemy = Enemy;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var AssaultEnemy = (function (_super) {
        __extends(AssaultEnemy, _super);
        function AssaultEnemy(game, x, y) {
            var _this = _super.call(this, game, x, y, 'scooter') || this;
            _this.timer = 2.5;
            _this.diving = false;
            _this.body.setSize(_this.width, _this.height);
            return _this;
        }
        AssaultEnemy.prototype.activate = function () {
            _super.prototype.activate.call(this);
            this.body.velocity.y = -this.player.baseSpeed * (400 / 500);
        };
        AssaultEnemy.prototype.dive = function (target) {
            if (target == null)
                return;
            this.diving = true;
            var angle = this.angleTo(target);
            this.targetVX = Math.cos(angle) * 400;
            this.targetVY = Math.sin(angle) * 400 - 500;
            if (this.targetVY < 0)
                this.targetVY *= -1;
        };
        AssaultEnemy.prototype.update = function () {
            _super.prototype.update.call(this);
            if (this.target == null) {
                var target = F84.GameState.Instance.objects.find(function (o) { return o.name == 'assaultTarget'; });
                if (target != null)
                    this.target = new Phaser.Point(target.centerX, target.centerY);
            }
            if (this.activated) {
                if (this.timer > 0 && !this.diving) {
                    this.timer -= F84.GameState.Instance.deltaTime;
                    if (this.timer <= 0) {
                        var enemies = F84.GameState.Instance.objects.filter(function (o) { return o instanceof AssaultEnemy; });
                        for (var _i = 0, enemies_1 = enemies; _i < enemies_1.length; _i++) {
                            var enemy = enemies_1[_i];
                            enemy.dive(this.target);
                        }
                    }
                }
                if (this.diving) {
                    this.body.velocity.x -= (this.body.velocity.x - this.targetVX) * 3 * F84.GameState.Instance.deltaTime;
                    this.body.velocity.y -= (this.body.velocity.y - this.targetVY) * 3 * F84.GameState.Instance.deltaTime;
                }
            }
        };
        return AssaultEnemy;
    }(F84.Enemy));
    F84.AssaultEnemy = AssaultEnemy;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var BackgroundSpawner = (function (_super) {
        __extends(BackgroundSpawner, _super);
        function BackgroundSpawner(game, camController, environment) {
            var _this = _super.call(this, game) || this;
            _this.game = game;
            _this.camera = camController;
            _this.threshold = camController.bounds.bottom + 1024;
            _this.queuedBackgrounds = [];
            _this.onLastSegmentSpawned = new Phaser.Signal();
            _this.setEnvironment(environment);
            _this.spawnNextSegment();
            return _this;
        }
        BackgroundSpawner.prototype.update = function () {
            while (this.camera.bounds.top < this.threshold + 20)
                this.spawnNextSegment();
        };
        BackgroundSpawner.prototype.setEnvironment = function (env) {
            var environments = this.game.cache.getJSON('environments');
            var str = env.valueOf();
            var environment = environments[str];
            this.bgPrefix = environment.bgPrefix;
            this.bgCount = environment.bgCount;
            this.bgIndex = 1;
            this.spawnForegrounds = environment.foreground;
        };
        BackgroundSpawner.prototype.switchEnvironment = function (env) {
            var prevEnvironment = this.bgPrefix;
            this.setEnvironment(env);
            var newEnvironment = this.bgPrefix;
            var name = prevEnvironment + '-' + newEnvironment;
            this.queueBackgrounds([name + '1', name + '2']);
        };
        BackgroundSpawner.prototype.queueBackgrounds = function (bgs) {
            this.queuedBackgrounds = bgs;
        };
        BackgroundSpawner.prototype.spawnNextSegment = function () {
            if (this.queuedBackgrounds.length > 0)
                this.spawnQueuedSegment();
            else
                this.spawnSequentialSegment();
        };
        BackgroundSpawner.prototype.spawnQueuedSegment = function () {
            var seg = this.queuedBackgrounds.shift();
            var bg = this.spawnImage(this.threshold - 1024, seg, F84.GameState.Instance.background);
            this.threshold -= bg.height;
        };
        BackgroundSpawner.prototype.spawnSequentialSegment = function () {
            var bottom = this.threshold - 1024;
            var bg = this.spawnImage(bottom, this.bgPrefix + this.bgIndex, F84.GameState.Instance.background);
            if (this.spawnForegrounds)
                this.spawnImage(bottom, this.bgPrefix + 'Top' + this.bgIndex, F84.GameState.Instance.foreground);
            this.bgIndex++;
            if (this.bgIndex > this.bgCount)
                this.bgIndex = 1;
            this.threshold -= bg.height;
            if (this.bgIndex == 1)
                this.onLastSegmentSpawned.dispatch();
        };
        BackgroundSpawner.prototype.spawnImage = function (bottom, key, group) {
            var _this = this;
            var bg = this.game.add.image(F84.GameState.Instance.bounds.centerX, bottom, key, null, group);
            bg.anchor.set(0.5, 1);
            bg.update = function () {
                if (bg.top > _this.camera.bounds.bottom)
                    bg.destroy();
            };
            if (key.includes('city')) {
                if (key == 'cityTop5') {
                    var light1 = this.game.add.image(-180, -bg.height / 2 + 170, 'streetLightFlicker');
                    bg.addChild(light1);
                    light1.anchor.set(0.5);
                    this.addFlicker(light1);
                    var light2 = this.game.add.image(180, -bg.height / 2 - 190, 'streetLightFlicker');
                    bg.addChild(light2);
                    light2.anchor.set(0.5);
                    this.addFlicker(light2);
                }
                else if (key == 'city10') {
                    var light = this.game.add.image(400, -300, 'billboardFlicker');
                    bg.addChild(light);
                    light.anchor.set(0.5);
                    light.scale.set(-1, 1);
                    this.game.add.tween(light).to({ alpha: 0 }, 500, Phaser.Easing.Quadratic.Out, true, 0, -1, true);
                }
                else if (key == 'city11') {
                    var light = this.game.add.image(423, -174, 'buildingFlicker');
                    bg.addChild(light);
                    light.anchor.set(0.5);
                    light.scale.set(-1, 1);
                    this.game.add.tween(light).to({ alpha: 0 }, 250, Phaser.Easing.Quadratic.Out, true, 0, -1, true);
                }
            }
            if (key.includes('construction')) {
                if (key.includes('Top')) {
                    if (key == 'constructionTop1') {
                        this.addCraneLights(bg, new Phaser.Point(101, -346));
                    }
                    else if (key == 'constructionTop3') {
                        this.addCraneLights(bg, new Phaser.Point(-149, -228));
                    }
                    else if (key == 'constructionTop4') {
                        this.addCraneLights(bg, new Phaser.Point(-228, -18));
                        this.addCraneLights(bg, new Phaser.Point(-294, -200));
                        this.addCraneLights(bg, new Phaser.Point(-146, -292));
                        this.addCraneLights(bg, new Phaser.Point(107, -356));
                    }
                    else if (key == 'constructionTop5') {
                        this.addWreckingBall(bg, new Phaser.Point(167, -87));
                    }
                    else if (key == 'constructionTop7') {
                        this.addWreckingBall(bg, new Phaser.Point(170, -263));
                    }
                    else if (key == 'constructionTop9') {
                        this.addCraneLights(bg, new Phaser.Point(-159, -575), true);
                    }
                    else if (key == 'constructionTop10') {
                        this.addWreckingBall(bg, new Phaser.Point(166, -525));
                    }
                    else if (key == 'constructionTop11') {
                        this.addWreckingBall(bg, new Phaser.Point(-223, -362));
                    }
                    else if (key == 'constructionTop12') {
                        this.addWreckingBall(bg, new Phaser.Point(138, -632));
                    }
                }
                else {
                    if (key == 'construction2') {
                        this.addCrateLights(bg, new Phaser.Point(301, -228));
                    }
                    else if (key == 'construction3') {
                        this.addCrateLights(bg, new Phaser.Point(229, -485));
                        this.addCrateLights(bg, new Phaser.Point(-299, -350));
                    }
                    else if (key == 'construction4') {
                        this.addCrateLights(bg, new Phaser.Point(-299, -120));
                    }
                    else if (key == 'construction5') {
                        this.addCrateLights(bg, new Phaser.Point(223, -169));
                        this.addCrateLights(bg, new Phaser.Point(-282, -235));
                    }
                    else if (key == 'construction6') {
                        this.addCrateLights(bg, new Phaser.Point(202, -372));
                    }
                    else if (key == 'construction7') {
                        this.addCrateLights(bg, new Phaser.Point(-279, -382));
                    }
                    else if (key == 'construction8') {
                        this.addCrateLights(bg, new Phaser.Point(-284, -567));
                    }
                    else if (key == 'construction10') {
                        this.addCrateLights(bg, new Phaser.Point(-228, -165));
                    }
                    else if (key == 'construction12') {
                        this.addCrateLights(bg, new Phaser.Point(-247, 10));
                        this.addCrateLights(bg, new Phaser.Point(221, -246));
                    }
                }
            }
            return bg;
        };
        BackgroundSpawner.prototype.addCrateLights = function (bg, pos) {
            var x = 1;
            if (pos.x >= 0) {
                pos.x += 16;
            }
            else {
                pos.x -= 23;
                x = -1;
            }
            pos.y -= 23;
            var frame = this.game.rnd.between(0, 14);
            for (var i = 0; i < 2; i++) {
                for (var n = 0; n < 2; n++) {
                    var light = this.game.add.sprite(pos.x + (70 * n * x), pos.y - (146 * i), 'ambientLight');
                    bg.addChild(light);
                    light.animations.add('').play(16, true);
                    light.animations.currentAnim.frame = frame;
                }
            }
        };
        BackgroundSpawner.prototype.addCraneLights = function (bg, pos, double) {
            if (double === void 0) { double = false; }
            if (pos.x >= 0) {
                pos.x += 16;
            }
            else {
                pos.x -= 23;
            }
            pos.y -= 23;
            var frame = this.game.rnd.between(0, 14);
            for (var i = 0; i < 2; i++) {
                var light = this.game.add.sprite(pos.x, pos.y - ((54 + (double ? 54 + 6 : 0)) * i), 'ambientLight');
                bg.addChild(light);
                light.animations.add('').play(16, true);
                light.animations.currentAnim.frame = frame;
            }
        };
        BackgroundSpawner.prototype.addWreckingBall = function (bg, pos) {
            var angle = this.game.rnd.sign() * this.game.rnd.between(10, 16);
            var ball = this.game.add.sprite(pos.x, pos.y, 'wreckingBall');
            ball.anchor.set(0.5, 0);
            ball.angle = angle;
            bg.addChild(ball);
            if (pos.x < 0) {
                ball.scale.x = -1;
            }
            var swing = this.game.add.tween(ball);
            swing.from({ angle: -angle }, 1500, Phaser.Easing.Quadratic.InOut, true, 0, -1, true);
        };
        BackgroundSpawner.prototype.addFlicker = function (sprite) {
            sprite.alpha = 0;
            var on1 = this.game.add.tween(sprite).to({ alpha: 1 }, 120, Phaser.Easing.Linear.None, true, this.game.rnd.between(0, 500));
            var off1 = this.game.add.tween(sprite).to({ alpha: 0 }, 120, Phaser.Easing.Linear.None);
            var on2 = this.game.add.tween(sprite).to({ alpha: 1 }, 120, Phaser.Easing.Linear.None);
            var off2 = this.game.add.tween(sprite).to({ alpha: 0 }, 1000, Phaser.Easing.Linear.None);
            on1.chain(off1);
            off1.chain(on2);
            on2.chain(off2);
            off2.chain(on1);
        };
        return BackgroundSpawner;
    }(Phaser.Group));
    F84.BackgroundSpawner = BackgroundSpawner;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Projectile = (function (_super) {
        __extends(Projectile, _super);
        function Projectile(game, x, y, key, width, height) {
            var _this = _super.call(this, game, x, y, key) || this;
            _this.life = 5;
            _this.rotateWithMovement = false;
            _this.anchor.set(0.5);
            game.physics.enable(_this);
            _this.body.setSize(width, height, _this.width / 2 - width / 2, _this.height / 2 - height / 2);
            return _this;
        }
        Projectile.prototype.update = function () {
            if (this.rotateWithMovement) {
                var vel = new Phaser.Point(this.body.velocity.x, this.body.velocity.y);
                this.rotation = Math.atan2(vel.y, vel.x) - Math.PI / 2;
            }
            var dt = F84.GameState.Instance.deltaTime;
            this.life -= dt;
            if (this.life <= 0)
                this.destroy();
        };
        return Projectile;
    }(Phaser.Sprite));
    F84.Projectile = Projectile;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Balloon = (function (_super) {
        __extends(Balloon, _super);
        function Balloon(game, x, y) {
            var _this = this;
            var sprite = game.rnd.pick(['balloon1', 'balloon2', 'balloon3']);
            _this = _super.call(this, game, x, y, sprite, 30, 30) || this;
            _this.body.velocity.x = game.rnd.between(-95, 95);
            _this.body.velocity.y = -200;
            _this.timer = 0;
            return _this;
        }
        Balloon.prototype.update = function () {
            this.timer += F84.GameState.Instance.deltaTime;
            this.body.x += Math.sin(this.timer * 2) * 60 * F84.GameState.Instance.deltaTime;
        };
        return Balloon;
    }(F84.Projectile));
    F84.Balloon = Balloon;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var BoostEffect = (function (_super) {
        __extends(BoostEffect, _super);
        function BoostEffect(game, player, vehicle) {
            var _this = _super.call(this, game, 0, 0, vehicle + 'Top', null) || this;
            _this.car = true;
            if (vehicle == 'batwing')
                _this.car = false;
            _this.offsets = [];
            _this.anchor.set(0.5, 0.5);
            if (_this.car)
                _this.offsets.push(new Phaser.Point(-1, 30));
            else
                _this.offsets.push(new Phaser.Point(-4, 40));
            _this.belowEffect = game.add.sprite(0, 0, vehicle + 'Below', null, F84.GameState.Instance.frontLayer);
            _this.belowEffect.anchor.set(0.5);
            if (_this.car)
                _this.offsets.push(new Phaser.Point(-1, 50));
            else
                _this.offsets.push(new Phaser.Point(-1, 70));
            var texture = player.vehicle === 'batwing' ? 'batPlaneShield' : 'batVehicleShield';
            _this.shieldOverlay = _this.game.add.sprite(0, 0, texture);
            _this.shieldOverlay.anchor.set(0.5);
            player.sprite.addChild(_this.shieldOverlay);
            _this.animations.add('').play(16, true);
            _this.belowEffect.animations.add('').play(16, true);
            _this.shieldOverlay.animations.add('play').play(14.5, true);
            _this.game.sound.play('boostActivate');
            return _this;
        }
        BoostEffect.prototype.update = function () {
            if (!this.car) {
                this.realignOffsets();
            }
            var player = F84.GameState.Instance.player.sprite;
            this.angle = player.angle;
            var position = player.position.clone().add(this.offsets[0].x, this.offsets[0].y);
            this.position = position.rotate(player.x, player.y, this.angle * (this.car ? 1 : 0.6), true, this.offsets[0].getMagnitude());
            var pos = player.position.clone().add(this.offsets[1].x, this.offsets[1].y);
            this.belowEffect.position = pos.rotate(player.x, player.y, this.angle, true, this.offsets[1].getMagnitude());
            this.belowEffect.angle = this.angle;
        };
        BoostEffect.prototype.realignOffsets = function () {
            var player = F84.GameState.Instance.player.sprite;
            var spriteAngle = player.angle;
            var angle = Math.sign(spriteAngle);
            if (angle == 0 && Math.sign(this.angle) == 0) {
                return;
            }
            for (var i = 0; i < this.offsets.length; i++) {
                var constant = (i == 0 ? -4 : -1);
                var key = player.key;
                if (key == 'batwingLeft')
                    constant -= (6 + i);
                else if (key == 'batwingRight') {
                    constant += (6 + i);
                }
                if (Math.abs(spriteAngle) > 3)
                    this.offsets[i].x = constant;
                else
                    this.offsets[i].x = constant - (3) * (angle) * angle * (spriteAngle / 23.6);
            }
        };
        BoostEffect.prototype.setAlpha = function (alpha) {
            this.belowEffect.alpha = alpha;
            this.alpha = alpha;
        };
        BoostEffect.prototype.finish = function () {
            F84.GameState.Instance.player.boostEffect = null;
            this.shieldOverlay.destroy();
            this.belowEffect.destroy();
            this.destroy();
        };
        return BoostEffect;
    }(Phaser.Sprite));
    F84.BoostEffect = BoostEffect;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Boss = (function (_super) {
        __extends(Boss, _super);
        function Boss(game, player, image) {
            var _this = _super.call(this, game, 0, 0, image) || this;
            _this.player = player;
            _this.maxHP = F84.GameState.Instance.getConfigValue('startingBossHealth', 'finalBossHealth');
            _this.hp = _this.maxHP;
            _this.data = { collides: true };
            _this.x = F84.GameState.Instance.bounds.centerX;
            _this.y = F84.GameState.Instance.camController.bounds.top - 100;
            _this.usedFinalAttack = false;
            _this.onDefeated = new Phaser.Signal();
            _this.tookDamage = new Phaser.Signal();
            return _this;
        }
        Boss.prototype.wait = function (time) {
            var _this = this;
            if (time === void 0) { time = 3; }
            var timer = time;
            return function () {
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        Boss.prototype.onHitObject = function (other) {
            if (other instanceof F84.PlayerMissile) {
                F84.GameState.Instance.destroyObject(other);
                this.loseHP(other);
            }
        };
        Boss.prototype.hit = function () {
            var _this = this;
            var timer = 1;
            this.loadTexture('bossAttack' + this.name);
            return function () {
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0) {
                    _this.loadTexture('boss' + _this.name);
                    _this.state = _this.wait(0.5);
                }
            };
        };
        Boss.prototype.intro = function (onComplete) {
            var _this = this;
            var update = this.update;
            var game = F84.GameState.Instance;
            var percentile = Math.abs(game.camController.bounds.top + 100 - this.y);
            this.state = this.wait(1000);
            this.update = function () {
                var position = game.camController.bounds.top + 100;
                _this.body.velocity.y = (1.2 * game.player.baseSpeed * (Math.abs(position - _this.y) / percentile));
                if (_this.y >= position) {
                    _this.y = position;
                    _this.state = _this.wait(2);
                    if (onComplete)
                        onComplete();
                    _this.update = update;
                }
            };
        };
        Boss.prototype.spinOut = function () {
            this.game.add.tween(this).to({ angle: this.angle + 360 * 4 }, 6000, Phaser.Easing.Quadratic.Out, true);
        };
        Boss.prototype.jettisonBoss = function (name) {
            var _this = this;
            var timer = this.game.time.create(true);
            timer.add(800, function () {
                var boss = _this.game.add.sprite(_this.x, _this.y, 'bossBroken' + name + '2');
                F84.GameState.Instance.addObject(boss, F84.GameState.Instance.foreground);
                boss.anchor.set(0.5);
                _this.game.physics.arcade.enable(boss);
                boss.body.velocity.y = -50;
                var swayTimer = 0;
                boss.update = function () {
                    swayTimer += F84.GameState.Instance.deltaTime;
                    boss.body.velocity.x = Math.sin(swayTimer * 2) * 100 + 50;
                };
            });
            timer.start();
        };
        Boss.prototype.die = function () {
            return function () { };
        };
        Boss.prototype.loseHP = function (missile) {
            this.hp--;
            if (this.hp < 0)
                this.hp = 0;
            this.tookDamage.dispatch();
            var ang = this.position.angle(missile.position);
            this.spawnExplosion(ang, 50);
            if (this.hp > 0)
                this.state = this.hit();
            else {
                this.onDefeated.dispatch();
                F84.GameState.Instance.ui.spawnBossDefeatedPopup(this.name);
                F84.GameState.Instance.bossKilled();
                this.state = this.die();
            }
        };
        Boss.prototype.spawnExplosion = function (angle, distance) {
            var _this = this;
            var explosion = this.game.add.sprite(0, 0, 'hitEffect' + this.name);
            this.events.onDestroy.add(function () { return explosion.destroy(); });
            explosion.anchor.set(0.5);
            explosion.update = function () {
                explosion.position.set(_this.x + Math.cos(angle) * distance, _this.y + Math.sin(angle) * distance);
            };
            explosion.update();
            explosion.animations.add('').play(16, false, true);
            this.game.sound.play('explosion' + this.game.rnd.integerInRange(1, 3), 0.25);
        };
        Boss.prototype.repeatExplosionSpawner = function () {
            var _this = this;
            var explosionTimer = 0;
            return function () {
                explosionTimer -= F84.GameState.Instance.deltaTime;
                if (explosionTimer <= 0) {
                    _this.spawnExplosion(_this.game.rnd.frac() * Math.PI * 2, _this.game.rnd.between(0, 80));
                    explosionTimer = _this.game.rnd.realInRange(0.2, 0.4);
                }
            };
        };
        Boss.prototype.angleToPlayer = function () {
            var dx = this.player.x - this.x;
            var dy = this.player.y - this.y;
            var angle = Math.atan2(dy, dx);
            return angle;
        };
        Boss.prototype.finalAttack = function () {
            return function () { };
        };
        Boss.prototype.nextState = function () {
            var attacks = [];
            attacks.push.apply(attacks, this.attacks);
            if (this.hp <= 1 && this.previousAttack != this.finalAttack)
                attacks.push(this.finalAttack);
            var attack = this.game.rnd.pick(attacks);
            if (this.hp <= 1 && !this.usedFinalAttack) {
                attack = this.finalAttack;
                this.usedFinalAttack = true;
            }
            this.state = attack.call(this);
            this.previousAttack = attack;
        };
        return Boss;
    }(Phaser.Sprite));
    F84.Boss = Boss;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var BouncingBullet = (function (_super) {
        __extends(BouncingBullet, _super);
        function BouncingBullet(game, x, y) {
            var _this = _super.call(this, game, x, y, 'hat', 30, 30) || this;
            _this.body.velocity.x = game.rnd.pick([-175, 175]);
            _this.body.velocity.y = -100;
            return _this;
        }
        BouncingBullet.prototype.update = function () {
            if (this.body.x < F84.GameState.Instance.bounds.centerX - F84.GameState.roadWidth / 2) {
                this.body.x = F84.GameState.Instance.bounds.centerX - F84.GameState.roadWidth / 2;
                this.body.velocity.x *= -1;
            }
            if (this.body.x + this.body.width > F84.GameState.Instance.bounds.centerX + F84.GameState.roadWidth / 2) {
                this.body.x = F84.GameState.Instance.bounds.centerX + F84.GameState.roadWidth / 2 - this.body.width;
                this.body.velocity.x *= -1;
            }
        };
        return BouncingBullet;
    }(F84.Projectile));
    F84.BouncingBullet = BouncingBullet;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Popup = (function (_super) {
        __extends(Popup, _super);
        function Popup(game) {
            var _this = _super.call(this, game) || this;
            _this.dismissing = false;
            return _this;
        }
        Popup.prototype.dismiss = function () {
            if (this.dismissing)
                return;
            this.dismissing = true;
            this.destroy();
        };
        Popup.prototype.isDismissed = function () {
            return this.dismissing;
        };
        return Popup;
    }(Phaser.Group));
    F84.Popup = Popup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Notification = (function (_super) {
        __extends(Notification, _super);
        function Notification(game, tweenPoint, tweenDuration) {
            var _this = _super.call(this, game) || this;
            _this.tweenPoint = tweenPoint;
            _this.tweenDuration = tweenDuration;
            return _this;
        }
        Notification.prototype.tweenIn = function () {
            var _this = this;
            var tween = this.game.add.tween(this);
            tween.from({ x: this.x + this.tweenPoint.x, y: this.y + this.tweenPoint.y }, this.tweenDuration, Phaser.Easing.Quadratic.Out, true);
            var timer = this.game.time.create();
            timer.add(1500, function () {
                _this.dismiss();
            });
            tween.onComplete.addOnce(function () { timer.start(); });
        };
        Notification.prototype.dismiss = function () {
            var _this = this;
            if (this.dismissing)
                return;
            this.dismissing = true;
            var tween = this.game.add.tween(this);
            tween.to({ x: this.x + this.tweenPoint.x, y: this.y + this.tweenPoint.y }, this.tweenDuration, Phaser.Easing.Quadratic.In, true);
            tween.onComplete.add(function () { return _this.destroy(); });
        };
        return Notification;
    }(F84.Popup));
    F84.Notification = Notification;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var BrickNotification = (function (_super) {
        __extends(BrickNotification, _super);
        function BrickNotification(game, powerup, count, max, reference) {
            var _this = _super.call(this, game, new Phaser.Point(-200, 0), 400) || this;
            var brickBG = _this.game.add.sprite(0, 0, 'hud' + powerup + 'Counter', null, _this);
            var style = { fontSize: 24, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var text = _this.game.add.text(0, 0, count + '/' + max, style, _this);
            text.alignIn(brickBG, Phaser.LEFT_CENTER, -60, 3);
            _this.alignTo(reference, Phaser.TOP_CENTER, 0, 9);
            _this.tweenIn();
            return _this;
        }
        return BrickNotification;
    }(F84.Notification));
    F84.BrickNotification = BrickNotification;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var CameraController = (function (_super) {
        __extends(CameraController, _super);
        function CameraController(game) {
            var _this = _super.call(this, game) || this;
            _this.game = game;
            _this.gameWidth = game.world.bounds.width;
            _this.gameHeight = game.world.bounds.height;
            _this.camera = game.camera;
            _this.targetScale = 1;
            _this.accel = 1;
            return _this;
        }
        CameraController.prototype.update = function () {
            var scaleFactor = Math.log(this.camScale);
            var targetFactor = Math.log(this.targetScale);
            scaleFactor -= (scaleFactor - targetFactor) * this.accel * this.game.time.physicsElapsed;
            this.setScale(Math.pow(Math.E, scaleFactor));
        };
        CameraController.prototype.postUpdate = function () {
            if (this.debugPts) {
                var bounds = this.bounds;
                this.debugPts[0].position.set(bounds.x, bounds.y);
                this.debugPts[1].position.set(bounds.x + bounds.width, bounds.y);
                this.debugPts[2].position.set(bounds.x, bounds.y + bounds.height);
                this.debugPts[3].position.set(bounds.x + bounds.width, bounds.y + bounds.height);
            }
        };
        CameraController.prototype.setScale = function (scale) {
            this.camera.scale.set(scale);
        };
        CameraController.prototype.setTargetScale = function (scale, accel) {
            if (accel === void 0) { accel = 1; }
            this.targetScale = scale;
            this.accel = accel;
        };
        Object.defineProperty(CameraController.prototype, "bounds", {
            get: function () {
                var width = this.gameWidth / this.camera.scale.x;
                var height = this.gameHeight / this.camera.scale.y;
                return new Phaser.Rectangle(this.camera.x / this.camScale, this.camera.y / this.camScale, width, height);
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(CameraController.prototype, "camScale", {
            get: function () {
                return this.camera.scale.y;
            },
            enumerable: true,
            configurable: true
        });
        CameraController.prototype.createDebugPoints = function () {
            this.debugPts = [];
            for (var i = 0; i < 4; i++) {
                this.debugPts[i] = this.game.add.sprite(0, 0, 'whiteSquare');
                var pt = this.debugPts[i];
                pt.width = 20;
                pt.height = 20;
                pt.anchor.set(0.5);
                pt.tint = 0xFF0000;
            }
        };
        return CameraController;
    }(Phaser.Group));
    F84.CameraController = CameraController;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var CameraTarget = (function (_super) {
        __extends(CameraTarget, _super);
        function CameraTarget(game, player) {
            var _this = _super.call(this, game, player.x, player.y - 250, 'whiteSquare') || this;
            _this.player = player;
            _this.alpha = 0;
            _this.followingPlayer = true;
            _this.vy = 0;
            return _this;
        }
        CameraTarget.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            if (this.followingPlayer) {
                this.position.set(this.player.x, this.player.y - 250);
            }
            else {
                this.y -= this.vy * F84.GameState.Instance.deltaTime;
            }
        };
        CameraTarget.prototype.stopFollowingPlayer = function () {
            this.followingPlayer = false;
            this.vy = this.player.speed / 2;
        };
        return CameraTarget;
    }(Phaser.Sprite));
    F84.CameraTarget = CameraTarget;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var CelebratoryMessage = (function (_super) {
        __extends(CelebratoryMessage, _super);
        function CelebratoryMessage(game) {
            var _this = _super.call(this, game, new Phaser.Point(-500, 0), 500) || this;
            var glow = game.add.sprite(0, 0, 'uiPacingGlow', null, _this);
            var message = F84.Strings.get(CelebratoryMessage.celebratoryKeys[game.rnd.between(0, 3)]);
            var style = { fontSize: 42, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var text = game.add.text(0, 0, message, style, _this);
            text.alignIn(glow, Phaser.LEFT_CENTER, -20, 0);
            _this.alignIn(F84.GameState.Instance.ui.bounds, Phaser.TOP_LEFT, 0, -180);
            _this.game.sound.play('streak');
            _this.tweenIn();
            return _this;
        }
        CelebratoryMessage.celebratoryKeys = [
            'GoodJob',
            'Awesome',
            'Amazing',
            'Great'
        ];
        return CelebratoryMessage;
    }(F84.Notification));
    F84.CelebratoryMessage = CelebratoryMessage;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ChunkSpawner = (function (_super) {
        __extends(ChunkSpawner, _super);
        function ChunkSpawner(game, camController) {
            var _this = _super.call(this, game) || this;
            _this.game = game;
            _this.camera = camController;
            _this.environment = F84.GameState.Instance.environment;
            _this.threshold = camController.bounds.top - 1024;
            _this.skipChunk = 0;
            _this.spawnBossChunks = false;
            _this.spawnPowerups = true;
            _this.onSpawnEnemy = new Phaser.Signal();
            return _this;
        }
        ChunkSpawner.prototype.update = function () {
            if (this.camera.bounds.top < this.threshold + 20)
                this.spawnChunk();
        };
        ChunkSpawner.prototype.spawnChunk = function () {
            if (this.skipChunk > 0) {
                this.skipChunk--;
                this.threshold -= 2048;
            }
            else {
                var i = this.game.rnd.between(1, 3);
                var chunkData = this.game.cache.getJSON('chunks');
                var environmentChunks = chunkData[this.environment.valueOf()];
                if (this.spawnBossChunks)
                    environmentChunks = chunkData.boss;
                var progress = F84.GameState.Instance.bossesKilled / F84.GameConfig.get('bossesUntilFinalValues');
                var chunks = [];
                chunks.push.apply(chunks, environmentChunks.easy);
                if (progress >= 1 / 3)
                    chunks.push.apply(chunks, environmentChunks.medium);
                if (progress >= 2 / 3)
                    chunks.push.apply(chunks, environmentChunks.hard);
                var chunk = this.game.cache.getJSON(this.game.rnd.pick(chunks));
                var chunkWidth = chunk.width * chunk.tilewidth;
                var chunkHeight = chunk.height * chunk.tileheight;
                var top_1 = this.threshold - chunkHeight;
                var objLayer = chunk.layers.find(function (l) { return l.name == 'objects'; });
                var obstacleLayer = chunk.layers.find(function (l) { return l.name == 'obstacles'; });
                if (obstacleLayer) {
                    var tileset = this.game.cache.getJSON('obstaclesTileset');
                    for (var _i = 0, _a = obstacleLayer.objects; _i < _a.length; _i++) {
                        var obj = _a[_i];
                        var x = obj.x + (F84.GameState.Instance.bounds.centerX - chunkWidth / 2);
                        var y = obj.y + top_1 - obj.height;
                        var tile = tileset.tiles[obj.gid - 1];
                        if (!tile)
                            continue;
                        var hitboxes = tile.objectgroup.objects;
                        var img = tile.image.substr(15, tile.image.length - 19);
                        this.addObject(new F84.Obstacle(this.game, x, y, hitboxes, img));
                    }
                }
                for (var _b = 0, _c = objLayer.objects; _b < _c.length; _b++) {
                    var obj = _c[_b];
                    var x = obj.x + (F84.GameState.Instance.bounds.centerX - chunkWidth / 2);
                    var y = obj.y + top_1;
                    if (obj.name == 'studs')
                        this.spawnStuds(obj, x, y);
                    if (obj.name == 'powerup') {
                        if (!this.spawnPowerups)
                            continue;
                        var spawn = (this.game.rnd.frac() < 0.6 || this.spawnBossChunks);
                        var type = void 0;
                        if (this.spawnBossChunks)
                            type = 'bossWeapon';
                        else
                            type = (this.game.rnd.frac() < 0.5) ? 'magnet' : 'boost';
                        if (spawn)
                            this.addObject(new F84.Powerup(this.game, x + obj.width / 2, y + obj.height / 2, type));
                    }
                    var enemy = false;
                    if (obj.name == 'swayEnemy') {
                        this.addObject(new F84.SwayEnemy(this.game, x + obj.width / 2, y + obj.width / 2), F84.GameState.Instance.frontLayer);
                        enemy = true;
                    }
                    if (obj.name == 'diveEnemy') {
                        this.addObject(new F84.DiveEnemy(this.game, x + obj.width / 2, y + obj.width / 2), F84.GameState.Instance.frontLayer);
                        enemy = true;
                    }
                    if (obj.name == 'assaultEnemy') {
                        this.addObject(new F84.AssaultEnemy(this.game, x + obj.width / 2, y + obj.width / 2), F84.GameState.Instance.frontLayer);
                        enemy = true;
                    }
                    if (enemy)
                        this.onSpawnEnemy.dispatch();
                    if (obj.name == 'assaultTarget') {
                        var target = this.game.add.sprite(x, y, 'whiteSquare');
                        target.alpha = 0;
                        target.anchor.set(0.5);
                        target.name = 'assaultTarget';
                        this.addObject(target);
                    }
                }
                this.threshold -= chunkHeight;
            }
        };
        ChunkSpawner.prototype.addObject = function (object, layer) {
            F84.GameState.Instance.addObject(object, layer);
        };
        ChunkSpawner.prototype.spawnStuds = function (obj, x, y) {
            var yStep = 18;
            for (var oy = 0; oy <= obj.height; oy += yStep) {
                var startX = (oy % (yStep * 2) == 0) ? 0 : 20;
                for (var ox = startX; ox <= obj.width; ox += 40) {
                    this.addObject(new F84.Stud(this.game, x + ox, y + oy));
                }
            }
        };
        return ChunkSpawner;
    }(Phaser.Group));
    F84.ChunkSpawner = ChunkSpawner;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var DiveEnemy = (function (_super) {
        __extends(DiveEnemy, _super);
        function DiveEnemy(game, x, y) {
            var _this = _super.call(this, game, x, y, 'pilot') || this;
            _this.timer = game.rnd.realInRange(1.5, 2.5);
            _this.diving = false;
            _this.body.setSize(_this.width, _this.height);
            return _this;
        }
        DiveEnemy.prototype.activate = function () {
            _super.prototype.activate.call(this);
            this.body.velocity.y = -this.player.baseSpeed * (350 / 500);
        };
        DiveEnemy.prototype.update = function () {
            _super.prototype.update.call(this);
            if (this.activated) {
                if (this.timer > 0) {
                    this.timer -= F84.GameState.Instance.deltaTime;
                    if (this.timer <= 0) {
                        this.diving = true;
                        this.targetAngle = this.angleToPlayer();
                    }
                }
                if (this.diving) {
                    this.body.velocity.x -= (this.body.velocity.x - Math.cos(this.targetAngle) * 250) * 3 * F84.GameState.Instance.deltaTime;
                    this.body.velocity.y -= (this.body.velocity.y - Math.sin(this.targetAngle) * 250) * 3 * F84.GameState.Instance.deltaTime;
                }
            }
        };
        return DiveEnemy;
    }(F84.Enemy));
    F84.DiveEnemy = DiveEnemy;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var EndTrigger = (function (_super) {
        __extends(EndTrigger, _super);
        function EndTrigger(game, x, y, width, height) {
            var _this = _super.call(this, game, x, y, 'whiteSquare') || this;
            _this.width = width;
            _this.height = height;
            _this.tint = 0x177d5f;
            _this.alpha = 0;
            game.physics.arcade.enable(_this);
            _this.body.immovable = true;
            _this.body.moves = false;
            return _this;
        }
        return EndTrigger;
    }(Phaser.Sprite));
    F84.EndTrigger = EndTrigger;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Environment;
    (function (Environment) {
        Environment["City"] = "city";
        Environment["Sky"] = "sky";
        Environment["Construction"] = "construction";
        Environment["Water"] = "water";
    })(Environment = F84.Environment || (F84.Environment = {}));
    var GameState = (function (_super) {
        __extends(GameState, _super);
        function GameState() {
            return _super !== null && _super.apply(this, arguments) || this;
        }
        Object.defineProperty(GameState.prototype, "deltaTime", {
            get: function () {
                if (this.isPaused)
                    return 0;
                return this.game.time.physicsElapsed;
            },
            enumerable: true,
            configurable: true
        });
        GameState.prototype.init = function (level) {
            this.level = level;
        };
        GameState.prototype.preload = function () {
            var preloader = this.preloader = new F84.PreloaderGroup(this.game);
            this.add.existing(preloader);
            F84.GameStatePreloader.preload.call(this);
        };
        GameState.prototype.loadUpdate = function (game) {
            _super.prototype.loadUpdate.call(this, game);
            var totalFiles = this.load.totalQueuedFiles() + this.load.totalLoadedFiles();
            var filesLoaded = this.load.totalLoadedFiles();
            var percentComplete = filesLoaded / totalFiles;
            this.preloader.setProgress(percentComplete);
        };
        GameState.prototype.create = function () {
            var _this = this;
            GameState.Instance = this;
            this.isPaused = false;
            this.oldWorldBounds = this.world.bounds.clone();
            this.camController = new F84.CameraController(this.game);
            var width = this.world.bounds.width;
            var centerX = this.world.bounds.width / 2;
            var boundsX = centerX - width / 2;
            var boundsY = 0;
            var bounds = this.bounds = new Phaser.Rectangle(boundsX, boundsY, width, F84.Game.Instance.DefaultHeight);
            this.environment = Environment.City;
            this.environmentTimer = 0;
            this.onSwitchEnvironment = new Phaser.Signal();
            this.bossPool = ['Freeze', 'Riddler', 'Joker'];
            this.setUpcomingBoss();
            this.bossActive = false;
            this.bossTimer = 60;
            this.bossesKilled = 0;
            this.score = 0;
            this.incrementScore = false;
            this.distance = 0;
            var worldWidth = 360;
            var worldHeight = 1024 * 500;
            this.world.setBounds(0, 0, worldWidth, 1024 * 500);
            this.timescale = new F84.Timescale(this.game, 60);
            this.physics.startSystem(Phaser.Physics.ARCADE);
            this.background = this.add.group();
            this.midGround = this.add.group();
            this.frontLayer = this.add.group();
            this.playerGroup = this.add.group();
            this.foreground = this.add.group();
            var player = this.player = new F84.Player(this.game);
            this.add.existing(player);
            player.alignIn(this.world.bounds, Phaser.BOTTOM_CENTER, 0, -40);
            player.x = this.bounds.centerX;
            this.distance = Math.floor(this.world.bounds.bottom - this.player.y / 10);
            player.onLose.addOnce(function () { return _this.lose(); });
            var camTarget = this.camTarget = new F84.CameraTarget(this.game, player);
            this.add.existing(camTarget);
            this.camera.focusOn(camTarget);
            this.camera.follow(camTarget);
            this.objects = [];
            this.backgroundSpawner = new F84.BackgroundSpawner(this.game, this.camController, this.environment);
            this.backgroundSpawner.onLastSegmentSpawned.add(this.onEnvironmentLoop, this);
            this.chunkSpawner = new F84.ChunkSpawner(this.game, this.camController);
            this.ui = new F84.GameUI(this.game);
            this.add.existing(this.ui);
            var next = function () {
                _this.chunkSpawner.skipChunk = 0;
                _this.distance = Math.floor(_this.world.bounds.bottom - _this.player.y / 10);
                _this.incrementScore = true;
            };
            this.chunkSpawner.skipChunk = 99999;
            if (!GameState.shownTutorial && !F84.GameConfig.get('skipTutorial')) {
                GameState.shownTutorial = true;
                this.ui.spawnMovementTutorial(next);
            }
            else {
                var replayTutorial_1 = false;
                var replay_1 = function () {
                    replayTutorial_1 = true;
                    this.ui.spawnMovementTutorial(next);
                }.bind(this);
                var btnGroup_1 = this.add.group();
                this.ui.add(btnGroup_1);
                var replayTutBtn_1 = this.add.button(0, 0, 'uiStoryNextBtn', replay_1, this, null, null, null, null, btnGroup_1);
                replayTutBtn_1.scale.set(0.85);
                replayTutBtn_1.alpha = 0.5;
                var replayString = F84.Strings.get('ReplayTutMobile');
                if (this.game.device.desktop)
                    replayString = F84.Strings.get('ReplayTutWeb');
                var style = { fill: 'white', fontSize: 25, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10, wordWrap: true, wordWrapWidth: 400, align: 'center' };
                var replayText = this.game.add.text(0, 0, '', style, btnGroup_1);
                replayText.setText(replayString);
                replayText.text = replayText.text.toLocaleUpperCase();
                replayText.lineSpacing = 10;
                replayText.alignIn(replayTutBtn_1, Phaser.BOTTOM_CENTER, 0, -3);
                btnGroup_1.alignIn(this.ui.bounds, Phaser.BOTTOM_CENTER, 2, -17);
                var fadeOut_1 = this.game.add.tween(btnGroup_1);
                fadeOut_1.to({ alpha: 0 }, 400, Phaser.Easing.Linear.None);
                fadeOut_1.onComplete.add(function () { return btnGroup_1.destroy(); });
                this.game.input.keyboard.onDownCallback = function (keypress) {
                    if (keypress.keyCode === Phaser.KeyCode.SPACEBAR) {
                        fadeOut_1.start();
                        _this.game.input.keyboard.removeCallbacks();
                        replay_1();
                    }
                };
                replayTutBtn_1.onInputDown.add(function () {
                    fadeOut_1.start();
                    _this.game.input.keyboard.removeCallbacks();
                });
                var timer = this.time.create(true);
                timer.add(3000, function () {
                    if (replayTutorial_1)
                        return;
                    fadeOut_1.start();
                    replayTutBtn_1.inputEnabled = false;
                    next();
                });
                timer.start();
            }
            this.chunkSpawner.onSpawnEnemy.addOnce(function () { return _this.ui.flashEnemyAlert(); });
            F84.Music.switchMusic(this.game, 'gameMusic');
        };
        GameState.prototype.addObject = function (obj, group) {
            var _this = this;
            if (group === void 0) { group = this.midGround; }
            if (group)
                group.add(obj);
            this.objects.push(obj);
            obj.events.onDestroy.add(function () { return F84.ArrayUtils.remove(_this.objects, obj); });
        };
        GameState.prototype.destroyObject = function (obj) {
            F84.ArrayUtils.remove(this.objects, obj);
            obj.destroy();
        };
        GameState.prototype.pauseButtonDown = function () {
            var menu = new F84.PauseMenu(this.game, this.unpause, this);
            this.add.existing(menu);
            this.pause();
        };
        GameState.prototype.pause = function () {
            this.isPaused = true;
            this.game.physics.arcade.isPaused = true;
            this.pauseAnimations(true);
            this.pauseTimers(true);
            this.game.tweens.pauseAll();
            var menu = new F84.PauseMenu(this.game, this.unpause, this);
            this.add.existing(menu);
        };
        GameState.prototype.unpause = function () {
            this.isPaused = false;
            this.game.physics.arcade.isPaused = false;
            this.pauseAnimations(false);
            this.pauseTimers(false);
            this.game.tweens.resumeAll();
        };
        GameState.prototype.pauseAnimations = function (value) {
            F84.AnimationList.animations.forEach(function (anim) {
                anim.paused = value;
            });
        };
        GameState.prototype.pauseTimers = function (value) {
            F84.TimerList.timers.forEach(function (timer) {
                timer.paused = value;
            });
        };
        GameState.prototype.update = function () {
            var dt = this.deltaTime;
            var suggestedFps = Math.max(Math.min(this.game.time.suggestedFps, 60), 30);
            if (suggestedFps != this.timescale.baseFPS) {
                this.timescale.setBaseFPS(suggestedFps);
            }
            var distance = Math.floor(this.world.bounds.bottom - this.player.y / 10);
            var diff = Math.max(Math.floor((distance - this.distance) / 10), 0);
            if (!this.bossActive && this.incrementScore)
                this.score += diff;
            if (diff > 0)
                this.distance = distance;
            this.environmentTimer += dt;
            if (this.bossTimer > 0) {
                this.bossTimer -= dt;
                if (this.bossTimer <= 0)
                    this.startBossSequence();
            }
            if (this.bossThreshold != null && this.camController.bounds.top < this.bossThreshold) {
                this.spawnBoss(this.upcomingBoss);
            }
            if (!this.isPaused) {
                var collideObjects = [];
                for (var _i = 0, _a = this.objects; _i < _a.length; _i++) {
                    var obj = _a[_i];
                    if (obj.data.collides)
                        collideObjects.push(obj);
                }
                this.physics.arcade.overlap(this.player, this.objects, this.player.onHitObject, null, this.player);
                this.physics.arcade.overlap(collideObjects, this.objects, this.overlap, null, this);
                if (this.player.transformation) {
                    var transform = this.player.transformation;
                    this.physics.arcade.overlap(transform.hitBox, this.objects, transform.onHitObject, null, transform);
                }
            }
            for (var _b = 0, _c = this.objects; _b < _c.length; _b++) {
                var obj = _c[_b];
                if (obj.top > this.camController.bounds.bottom + 200) {
                    if (obj instanceof F84.Stud && !obj.collected) {
                        this.player.studCombo = 0;
                    }
                    this.destroyObject(obj);
                }
            }
        };
        GameState.prototype.overlap = function (obj1, obj2) {
            var o1 = obj1;
            if (o1.onHitObject)
                o1.onHitObject(obj2);
        };
        GameState.prototype.onEnvironmentLoop = function () {
            if (this.environmentTimer >= 25 && !this.bossActive)
                this.switchEnvironment();
        };
        GameState.prototype.switchEnvironment = function () {
            var _this = this;
            var UpcomingBoss = { Freeze: F84.FreezeBoss, Riddler: F84.RiddlerBoss, Joker: F84.JokerBoss }[this.upcomingBoss];
            var bossWantsThisEnvironment = UpcomingBoss.environment.find(function (e) { return e == _this.environment; });
            if (bossWantsThisEnvironment && this.bossTimer <= 15)
                return;
            var cycle = [Environment.City, Environment.Sky, Environment.Construction, Environment.Water];
            var i = (cycle.indexOf(this.environment) + 1) % cycle.length;
            this.environment = cycle[i];
            this.backgroundSpawner.switchEnvironment(this.environment);
            this.chunkSpawner.environment = this.environment;
            this.chunkSpawner.threshold = this.backgroundSpawner.threshold - 512;
            this.chunkSpawner.skipChunk = 1;
            this.environmentTimer = 0;
            var objects = F84.ArrayUtils.clone(this.objects);
            for (var _i = 0, objects_1 = objects; _i < objects_1.length; _i++) {
                var obj = objects_1[_i];
                if (obj instanceof F84.Obstacle && obj.bottom < this.camController.bounds.top)
                    this.destroyObject(obj);
            }
            if (this.environment == Environment.Sky || this.environment == Environment.Water) {
                this.player.transformThreshold = this.backgroundSpawner.threshold - 550;
            }
            else
                this.player.transformThreshold = this.backgroundSpawner.threshold - 2300;
            this.onSwitchEnvironment.dispatch();
        };
        GameState.prototype.playTransformationAnimation = function (vehicle) {
            var transformation;
            if (vehicle == 'batwing')
                transformation = new F84.VehicleTransformation(this.game, 'transformBatmobile', 'transformBatwing', 'batwing');
            if (vehicle == 'batmobile')
                transformation = new F84.VehicleTransformation(this.game, 'transformBatwing', 'transformBatmobile', 'batmobile');
            this.foreground.add(transformation);
            this.player.transformation = transformation;
        };
        GameState.prototype.setUpcomingBoss = function () {
            this.upcomingBoss = this.rnd.pick(this.bossPool);
            F84.ArrayUtils.remove(this.bossPool, this.upcomingBoss);
            if (this.bossPool.length == 0)
                this.bossPool = ['Freeze', 'Riddler', 'Joker'];
        };
        GameState.prototype.startBossSequence = function () {
            var _this = this;
            var start = function (offset) {
                _this.bossTimer = 0;
                _this.chunkSpawner.spawnBossChunks = true;
                _this.chunkSpawner.spawnPowerups = false;
                var threshold = _this.chunkSpawner.threshold - offset;
                _this.bossThreshold = threshold;
            };
            var Boss = { Freeze: F84.FreezeBoss, Riddler: F84.RiddlerBoss, Joker: F84.JokerBoss }[this.upcomingBoss];
            if (Boss.environment.find(function (e) { return e == _this.environment; })) {
                if (this.player.transformThreshold) {
                    console.log('Boss being spawned in environment that has not changed yet');
                    var threshold = Math.abs(this.player.transformThreshold - this.chunkSpawner.threshold);
                    start(threshold + 1800);
                }
                else
                    start(500);
            }
            else {
                var offset = Math.abs(this.player.body.y - this.camController.bounds.top);
                if (this.environment == Environment.Sky || this.environment == Environment.Water) {
                    var transformThreshold_1 = offset + 1788;
                    this.onSwitchEnvironment.addOnce(function () { return start(transformThreshold_1 + 1100); });
                }
                else {
                    var transformThreshold_2 = offset + 38;
                    this.onSwitchEnvironment.addOnce(function () { return start(transformThreshold_2 + 1500); });
                }
            }
        };
        GameState.prototype.spawnBoss = function (bossName) {
            var _this = this;
            this.currentBoss = this.upcomingBoss;
            this.bossThreshold = null;
            this.bossTimer = 0;
            this.chunkSpawner.spawnBossChunks = true;
            this.chunkSpawner.spawnPowerups = false;
            var bossSpawnTimer = this.time.create(true);
            bossSpawnTimer.add(2000, function () {
                var Boss = { Freeze: F84.FreezeBoss, Riddler: F84.RiddlerBoss, Joker: F84.JokerBoss }[bossName];
                var boss = new Boss(_this.game, _this.player);
                _this.add.existing(boss);
                _this.addObject(boss, _this.frontLayer);
                _this.bossActive = true;
                _this.ui.enableBossHPBar(boss);
                _this.chunkSpawner.spawnPowerups = true;
            });
            bossSpawnTimer.start();
            this.ui.flashBossAlert(this.currentBoss);
        };
        GameState.prototype.getConfigValue = function (key1, key2) {
            var start = F84.GameConfig.get(key1);
            var end = F84.GameConfig.get(key2);
            var bosses = F84.GameConfig.get('bossesUntilFinalValues');
            var frac = Math.min(this.bossesKilled / bosses, 1);
            return start + (end - start) * frac;
        };
        GameState.prototype.bossKilled = function () {
            this.bossActive = false;
            this.bossTimer = 60;
            this.score += 100;
            this.ui.disableBossHPBar();
            this.chunkSpawner.spawnBossChunks = false;
            this.bossesKilled++;
            this.setUpcomingBoss();
            this.player.baseSpeed = this.getConfigValue('startingMovementSpeed', 'finalMovementSpeed');
            this.player.speed = this.player.baseSpeed;
            for (var _i = 0, _a = this.objects; _i < _a.length; _i++) {
                var obj = _a[_i];
                if (obj instanceof F84.Powerup && obj.powerupType == 'bossWeapon')
                    obj.pendingDestroy = true;
            }
        };
        GameState.prototype.lose = function () {
            var _this = this;
            this.camTarget.stopFollowingPlayer();
            this.ui.spawnGameOverPopup();
            var timer = this.time.create();
            timer.add(2000, function () {
                var data = {
                    studsEarned: _this.player.studs,
                    score: _this.score,
                    highScore: false,
                };
                var saveData = F84.PlayerData.Instance.saveData;
                saveData.studs += data.studsEarned;
                if (saveData.highScore < data.score) {
                    saveData.highScore = data.score;
                    data.highScore = true;
                }
                F84.PlayerData.Instance.save();
                _this.game.state.start('RecapScreen', true, false, data);
            });
            timer.start();
        };
        GameState.prototype.shutdown = function () {
            this.world.setBounds(this.oldWorldBounds.x, this.oldWorldBounds.y, this.oldWorldBounds.width, this.oldWorldBounds.height);
        };
        GameState.shownTutorial = false;
        GameState.roadWidth = 480;
        return GameState;
    }(F84.ResizableState));
    F84.GameState = GameState;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var FreezeBoss = (function (_super) {
        __extends(FreezeBoss, _super);
        function FreezeBoss(game, player) {
            var _this = _super.call(this, game, player, 'bossFreeze') || this;
            _this.name = 'Freeze';
            _this.anchor.set(0.5);
            game.physics.enable(_this);
            _this.timer = 0;
            _this.movingX = true;
            _this.movingY = false;
            _this.attacks = [_this.snowballAttack, _this.icicleAttack];
            _this.intro(function () { _this.movingX = true; _this.movingY = true; });
            return _this;
        }
        FreezeBoss.prototype.update = function () {
            var dt = F84.GameState.Instance.deltaTime;
            if (this.movingX) {
                this.timer += dt;
                this.x = F84.GameState.Instance.bounds.centerX + 150 * Math.sin(this.timer);
            }
            else {
                this.body.velocity.x -= (this.body.velocity.x) * 4 * dt;
            }
            this.state();
        };
        FreezeBoss.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            if (this.movingY)
                this.y = F84.GameState.Instance.camController.bounds.top + 100;
        };
        FreezeBoss.prototype.snowballAttack = function () {
            var _this = this;
            var timer = 4;
            var snowballsRemaining = 3;
            var spawnTimer = 0;
            var spawnSnowball = function () {
                var snowball = new F84.Snowball(_this.game, _this.x, _this.y, 0, 0);
                _this.game.add.existing(snowball);
                F84.GameState.Instance.addObject(snowball);
                snowballsRemaining--;
                _this.game.sound.play('freezeSnowball');
            };
            return function () {
                spawnTimer -= F84.GameState.Instance.deltaTime;
                if (spawnTimer <= 0 && snowballsRemaining > 0) {
                    spawnSnowball();
                    spawnTimer += 0.5;
                }
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        FreezeBoss.prototype.icicleAttack = function () {
            var _this = this;
            var timer = 4;
            var iciclesRemaining = 4;
            var waveTimer = 0.8;
            var wavesRemaining = 1;
            var spawnTimer = 0;
            var spawnIcicle = function () {
                var angle = _this.angleToPlayer();
                var icicle = new F84.Projectile(_this.game, _this.x, _this.y, 'icicle', 20, 20);
                icicle.body.velocity.set(Math.cos(angle) * 350, Math.sin(angle) * 350);
                _this.game.add.existing(icicle);
                F84.GameState.Instance.addObject(icicle);
                iciclesRemaining--;
                _this.game.sound.play('freezeShards');
            };
            return function () {
                waveTimer -= F84.GameState.Instance.deltaTime;
                if (waveTimer <= 0 && wavesRemaining > 0) {
                    waveTimer += 1.35;
                    spawnTimer = 0;
                    iciclesRemaining = 4;
                    wavesRemaining--;
                }
                spawnTimer -= F84.GameState.Instance.deltaTime;
                if (spawnTimer <= 0 && iciclesRemaining > 0) {
                    spawnIcicle();
                    spawnTimer += 0.12;
                }
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        FreezeBoss.prototype.finalAttack = function () {
            var _this = this;
            var timer = 6;
            this.movingX = false;
            var freezeRay = new F84.FreezeRay(this.game, this, this.player);
            F84.GameState.Instance.midGround.add(freezeRay);
            this.onDefeated.add(function () { return freezeRay.destroy(); });
            return function () {
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0) {
                    _this.movingX = true;
                    _this.nextState();
                }
            };
        };
        FreezeBoss.prototype.die = function () {
            var _this = this;
            var timer = 10;
            this.loadTexture('bossBrokenFreeze');
            this.movingX = false;
            this.movingY = false;
            if (this.body.x < F84.GameState.Instance.bounds.centerX)
                this.body.velocity.x = 100;
            else
                this.body.velocity.x = -100;
            this.body.velocity.y = 0;
            this.body.enable = false;
            var spawnRepeatExplosions = this.repeatExplosionSpawner();
            this.spinOut();
            return function () {
                timer -= F84.GameState.Instance.deltaTime;
                spawnRepeatExplosions();
                if (timer <= 0)
                    _this.destroy();
            };
        };
        FreezeBoss.environment = [F84.Environment.City, F84.Environment.Construction];
        return FreezeBoss;
    }(F84.Boss));
    F84.FreezeBoss = FreezeBoss;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var FreezeRay = (function (_super) {
        __extends(FreezeRay, _super);
        function FreezeRay(game, boss, player) {
            var _this = _super.call(this, game) || this;
            _this.boss = boss;
            _this.player = player;
            _this.position.set(_this.game.camera.x, _this.game.camera.y);
            var playerPos = new Phaser.Point(player.x - _this.x, player.y - _this.y);
            _this.targetPos = playerPos;
            var target = _this.target = _this.game.add.sprite(player.x, player.y, 'freezeTarget', null, F84.GameState.Instance.foreground);
            target.anchor.set(0.5);
            _this.timer = 5;
            _this.shootTimer = 0;
            return _this;
        }
        FreezeRay.prototype.update = function () {
            if (this.target)
                this.target.position.set(this.targetPos.x + this.x, this.targetPos.y + this.y);
            var dt = F84.GameState.Instance.deltaTime;
            this.timer -= dt;
            if (this.timer <= 2.5 && this.timer > 0.5) {
                if (this.target)
                    this.target.destroy();
                this.shootTimer -= dt;
                if (this.shootTimer <= 0) {
                    var bossPos = new Phaser.Point(this.boss.x - this.x, this.boss.y - this.y);
                    var angle = Math.atan2(this.targetPos.y - bossPos.y, this.targetPos.x - bossPos.x);
                    this.shootTimer += 0.05;
                    var projectile = new F84.Projectile(this.game, bossPos.x + this.x + this.game.rnd.between(-10, 10), bossPos.y + this.y + this.game.rnd.between(-10, 10) + 60, 'freezeRay', 20, 20);
                    projectile.anchor.set(0.5);
                    F84.GameState.Instance.addObject(projectile, F84.GameState.Instance.frontLayer);
                    projectile.body.velocity.x = Math.cos(angle) * 900;
                    projectile.body.velocity.y = Math.sin(angle) * 900;
                    projectile.rotateWithMovement = true;
                    this.game.sound.play('freezeBeam');
                }
            }
            if (this.timer <= 0) {
                this.destroy();
            }
        };
        FreezeRay.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            this.position.set(this.game.camera.x, this.game.camera.y);
        };
        return FreezeRay;
    }(Phaser.Group));
    F84.FreezeRay = FreezeRay;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameFactory = (function () {
        function GameFactory() {
        }
        return GameFactory;
    }());
    F84.GameFactory = GameFactory;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameStatePreloader = (function () {
        function GameStatePreloader() {
        }
        GameStatePreloader.preload = function () {
            var _this = this;
            if (GameStatePreloader.loadedAssets)
                return;
            this.loadImages('./assets/images/game/', {
                batmobile: 'lbm-batman-car',
                batmobileLeft: 'lbm-batman-car-left',
                batmobileRight: 'lbm-batman-car-right',
                batmobileHit: 'lbm-batman-attackcar',
                batwing: 'lbm-batman-batwing',
                batwingLeft: 'lbm-batman-batwing-left',
                batwingRight: 'lbm-batman-batwing-right',
                batwingHit: 'lbm-batman-attack-batwing',
                droneFreeze: 'drone-frozer-00',
                droneJoker: 'drone-joker-00',
                droneRiddler: 'drone-riddler-00',
                pilotFreeze: 'enemy-pilot-freeze',
                pilotJoker: 'enemy-pilot-joker',
                pilotRiddler: 'enemy-pilot-riddler',
                scooterFreeze: 'enemy-scooter-freeze',
                scooterJoker: 'enemy-scooter-joker',
                scooterRiddler: 'enemy-scooter-riddler',
                bossFreeze: 'enemy-boss-freeze',
                bossAttackFreeze: 'enemy-boss-attack-freeze',
                bossBrokenFreeze: 'enemy-boss-broken-freeze',
                bossJoker: 'enemy-boss-joker',
                bossAttackJoker: 'enemy-boss-attack-joker',
                bossBrokenJoker: 'enemy-boss-broken-joker',
                bossBrokenJoker2: 'enemy-boss-broken-joker02',
                bossRiddler: 'villain-riddler-00',
                bossAttackRiddler: 'enemy-boss-attack-riddler',
                bossBrokenRiddler: 'enemy-boss-broken-riddler',
                bossBrokenRiddler2: 'enemy-boss-broken-riddler02',
                icicle: 'enemy-boss-freeze-icicle-small',
                snowball: 'enemy-boss-freeze-snowball',
                freezeTarget: 'enemy-boss-freeze-target',
                freezeRay: 'enemy-boss-freeze-ice-beam-component',
                riddlerBullet: 'enemy-boss-riddler-bullet',
                riddlerBulletGlow: 'enemy-boss-riddler-bullet-glow',
                hat: 'enemy-boss-riddler-hat',
                riddlerBoxClosed: 'enemy-boss-riddler-box-closed',
                riddlerBoxOpen: 'enemy-boss-riddler-box-open',
                riddlerShockwaveTop: 'enemy-boss-riddler-explosion-bottom',
                riddlerShockwaveBottom: 'enemy-boss-riddler-explosion-bottom',
                balloon1: 'enemy-boss-joker-balloon-blue',
                balloon2: 'enemy-boss-joker-balloon-red',
                balloon3: 'enemy-boss-joker-balloon-yellow',
                jokerBullet1: 'enemy-boss-joker-bullet-long',
                jokerBullet2: 'enemy-boss-joker-bullet-med',
                jokerBullet3: 'enemy-boss-joker-bullet-short',
                jokerBulletGlow: 'enemy-boss-joker-bullet-long-glow',
                jokerTarget: 'enemy-boss-joker-target',
                jokerGlove: 'enemy-boss-joker-glove',
                jokerGloveBack: 'enemy-boss-joker-glove-back',
                rocket: 'lbm-rocket',
                boostPiece1: 'lbm-power-up-booster-block-1',
                boostPiece2: 'lbm-power-up-booster-block-2',
                boostPiece3: 'lbm-power-up-booster-block-3',
                boostPiece4: 'lbm-power-up-booster-block-4',
                boostGlow: 'lbm-power-up-booster-glow',
                magnetPiece1: 'lbm-power-up-magnet-block-1',
                magnetPiece2: 'lbm-power-up-magnet-block-2',
                magnetPiece3: 'lbm-power-up-magnet-block-3',
                magnetPiece4: 'lbm-power-up-magnet-block-4',
                magnetGlow: 'lbm-power-up-magnet-glow',
                bossWeaponPiece1: 'lbm-power-up-missile-block-1',
                bossWeaponPiece2: 'lbm-power-up-missile-block-2',
                bossWeaponPiece3: 'lbm-power-up-missile-block-3',
                bossWeaponPiece4: 'lbm-power-up-missile-block-4',
                bossWeaponGlow: 'lbm-power-up-missile-glow',
                magnetEffect: 'lbm-animation-particle-magnet-static',
            });
            var loadAnimation = function (name, path, img, json) {
                if (json === void 0) { json = null; }
                _this.load.atlasJSONArray(name, path + img + '.png', path + (json ? json : img) + '.json');
            };
            var loadAnimations = function (path, map) {
                for (var name_5 in map) {
                    if (map[name_5].length > 0) {
                        var img = map[name_5][0];
                        if (map[name_5].length >= 2)
                            loadAnimation(name_5, path, img, map[name_5][1]);
                        else
                            loadAnimation(name_5, path, img);
                    }
                    else
                        continue;
                }
            };
            loadAnimations('./assets/images/game/', {
                vehicleTrail: ['vehicle-trail'],
                rocketTrail: ['rocket-trail'],
                stud: ['stud'],
                studCollect: ['stud-collect'],
                droneFreezeBlades: ['drone-freeze-blades'],
                droneJokerBlades: ['drone-joker-blades'],
                droneRiddlerBlades: ['drone-riddler-blades'],
                riddlerBlades: ['riddler-blades'],
                magnetBrick: ['magnet-brick'],
                boostBrick: ['boost-brick'],
                bossWeaponBrick: ['rocket-brick'],
                hitEffect: ['hit-effect'],
                hitEffectFreeze: ['hit-effect-freeze'],
                hitEffectRiddler: ['hit-effect-riddler'],
                hitEffectJoker: ['hit-effect-joker'],
                airStreak1: ['air-streak-1'],
                airStreak2: ['air-streak-2'],
                riddlerCube: ['riddler-cube'],
                smoke1: ['smoke-v1'],
                smoke2: ['smoke-v2'],
                warningLight: ['construction/const-warning-light'],
                ambientLight: ['construction/const-ambient-light'],
                batmobileBelow: ['boost-car-below'],
                batmobileTop: ['boost-car-top'],
                batwingBelow: ['boost-wing-below'],
                batwingTop: ['boost-wing-top'],
                powerUpCollection: ['power-up-collection'],
                batVehicleShield: ['batvehicle-shield'],
                batPlaneShield: ['batplane-shield'],
            });
            loadAnimations('./assets/images/game/transform/transform-', {
                transformBatmobile: ['car'],
                transformBatwing: ['batwing']
            });
            this.loadImages('./assets/images/game/city/lbm_enviro_city_', {
                city1: 'base_01',
                city2: 'base_02',
                city3: 'base_03',
                city4: 'base_04',
                city5: 'base_05',
                city6: 'base_06',
                city7: 'base_07',
                city8: 'base_08',
                city9: 'base_09',
                city10: 'base_10',
                city11: 'base_11',
                city12: 'base_12',
                cityTop1: 'top_01',
                cityTop2: 'top_02',
                cityTop3: 'top_03',
                cityTop4: 'top_04',
                cityTop5: 'top_05',
                cityTop6: 'top_06',
                cityTop7: 'top_07',
                cityTop8: 'top_08',
                cityTop9: 'top_09',
                cityTop10: 'top_10',
                cityTop11: 'top_11',
                cityTop12: 'top_12',
            });
            this.loadImages('./assets/images/game/city/', {
                'city-sky1': 'lbm_enviro_trans_sky_02',
                'city-sky2': 'lbm_enviro_trans_sky_01',
                'sky-construction1': 'lbm_enviro_trans_re_sky_02',
                'sky-construction2': 'lbm_enviro_trans_re_sky_01',
                'construction-water1': 'lbm_enviro_trans_city_02',
                'construction-water2': 'lbm_enviro_trans_water_01',
                'water-city1': 'lbm_enviro_trans_re_water_02',
                'water-city2': 'lbm_enviro_trans_re_water_01',
                'streetLightFlicker': 'lbm-street-light-flicker-1',
                'billboardFlicker': 'lbm-building-billboard-flicker-1',
                'buildingFlicker': 'lbm-building-flicker-1',
            });
            this.loadImages('./assets/images/game/construction/lbm_enviro_const_', {
                construction1: 'base_01',
                construction2: 'base_02',
                construction3: 'base_03',
                construction4: 'base_04',
                construction5: 'base_05',
                construction6: 'base_06',
                construction7: 'base_07',
                construction8: 'base_08',
                construction9: 'base_09',
                construction10: 'base_10',
                construction11: 'base_11',
                construction12: 'base_12',
                constructionTop1: 'top_01',
                constructionTop2: 'top_02',
                constructionTop3: 'top_03',
                constructionTop4: 'top_04',
                constructionTop5: 'top_05',
                constructionTop6: 'top_06',
                constructionTop7: 'top_07',
                constructionTop8: 'top_08',
                constructionTop9: 'top_09',
                constructionTop10: 'top_10',
                constructionTop11: 'top_11',
                constructionTop12: 'top_12',
            });
            this.loadImages('./assets/images/game/construction/lbm-const-', {
                wreckingBall: 'swining-wrecking-ball',
            });
            this.loadImages('./assets/images/game/sky/lbm_enviro_sky_', {
                sky1: 'base_01',
                sky2: 'base_02',
                sky3: 'base_03',
                sky4: 'base_04',
                sky5: 'base_05',
                sky6: 'base_06',
                sky7: 'base_07',
                sky8: 'base_08',
                sky9: 'base_09',
                sky10: 'base_10',
                sky11: 'base_11',
                sky12: 'base_12',
            });
            this.loadImages('./assets/images/game/water/lbm_enviro_water_', {
                water1: 'base_01',
                water2: 'base_02',
                water3: 'base_03',
                water4: 'base_04',
                water5: 'base_05',
                water6: 'base_06',
                water7: 'base_07',
                water8: 'base_08',
                water9: 'base_09',
                water10: 'base_10',
                water11: 'base_11',
                water12: 'base_12',
            });
            this.loadImagesVerbatim('./assets/images/game/', [
                'city/lbm_enviro_city_obstacle_01',
                'city/lbm_enviro_city_obstacle_02',
                'city/lbm_enviro_city_obstacle_02_light',
                'city/lbm_enviro_city_obstacle_03',
                'city/lbm_enviro_city_obstacle_04',
                'city/lbm_enviro_city_obstacle_05',
                'city/lbm_enviro_city_obstacle_06',
                'city/lbm_enviro_city_obstacle_07',
                'city/lbm_enviro_city_obstacle_08',
                'city/lbm_enviro_city_hazards_lights',
                'sky/lbm_enviro_sky_obstacle-01',
                'sky/lbm_enviro_sky_obstacle-02',
                'sky/lbm_enviro_sky_obstacle-03',
                'sky/lbm_enviro_sky_obstacle-04',
                'sky/lbm_enviro_sky_obstacle-05',
                'sky/lbm_enviro_sky_obstacle-06',
                'construction/lbm_enviro_const_obstacle_01',
                'construction/lbm_enviro_const_obstacle_02',
                'construction/lbm_enviro_const_obstacle_03',
                'construction/lbm_enviro_const_obstacle_04',
                'construction/lbm_enviro_const_obstacle_05',
                'construction/lbm_enviro_const_obstacle_06',
                'water/lbm_enviro_water_obstacle-01',
                'water/lbm_enviro_water_obstacle-02',
                'water/lbm_enviro_water_obstacle-03',
                'water/lbm_enviro_water_obstacle-04',
                'water/lbm_enviro_water_obstacle-05',
            ]);
            this.loadImages('./assets/images/game/transform/lbm-', {
                transformBatman1: 'batman01',
                transformBatman2: 'batman02',
                transformBatwing1: 'batwing01',
                transformBatwing2: 'batwing02',
                transformBatwing3: 'batwing03',
                transformBatwingComp1: 'bat-wing-component01',
                transformBatwingComp2: 'bat-wing-component02',
                transformBatwingComp3: 'bat-wing-component03',
                transformBatwingComp4: 'bat-wing-component04',
                transformBatmobile1: 'car01',
                transformBatmobile2: 'car02',
                transformBatmobile3: 'car03',
                transformComponent01: 'component01',
                transformComponent02: 'component02',
                transformComponent03: 'component03',
                transformComponent04: 'component04',
                transformComponent05: 'component05',
                transformComponent06: 'component06',
                transformComponent07: 'component07',
                transformComponent08: 'component08',
                transformComponent09: 'component09',
                transformComponent10: 'component10',
                transformComponent11: 'component11',
                transformComponent12: 'component12',
                transformGlow: 'glow',
                transformLogo: 'logo',
            });
            GameStatePreloader.loadedAssets = true;
        };
        GameStatePreloader.loadedAssets = false;
        return GameStatePreloader;
    }());
    F84.GameStatePreloader = GameStatePreloader;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var GameUI = (function (_super) {
        __extends(GameUI, _super);
        function GameUI(game) {
            var _this = _super.call(this, game) || this;
            var bounds = _this.bounds = F84.GameState.Instance.bounds;
            bounds = F84.UIUtils.getMobileBounds(bounds);
            var hpBG = game.add.sprite(0, 0, 'hudHealth' + F84.GameState.Instance.player.maxHP, null, _this);
            var hpIcon = _this.hpIcon = game.add.sprite(0, 0, 'hudHealthIcon', null, _this);
            hpIcon.alignIn(bounds, Phaser.TOP_LEFT, -12, -7);
            hpBG.alignTo(hpIcon, Phaser.RIGHT_CENTER, -110, -42);
            _this.hpDots = [];
            _this.bossHpDots = [];
            F84.GameState.Instance.player.tookDamage.add(_this.playerDamaged, _this);
            for (var i = 0; i < F84.GameState.Instance.player.maxHP; i++) {
                var hp = game.add.sprite(0, 0, 'hudHealthDot', null, _this);
                hp.alignIn(hpBG, Phaser.TOP_LEFT, -55 - 30 * i, -12);
                _this.hpDots.push(hp);
            }
            var scoreBG = _this.scoreBG = game.add.sprite(0, 0, 'hudScoreBG', null, _this);
            scoreBG.alignIn(bounds, Phaser.TOP_RIGHT, -123, -21);
            var scoreHeaderStyle = { fontSize: 18, fill: F84.GameColors.ORANGE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var scoreHeader = game.add.text(20, 50, F84.Strings.get('UIScore'), scoreHeaderStyle, _this);
            scoreHeader.alignIn(scoreBG, Phaser.LEFT_CENTER, -15, 1);
            var style = { fontSize: 24, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            _this.score = game.add.text(20, 50, '0', style, _this);
            _this.score.alignTo(scoreHeader, Phaser.RIGHT_CENTER, 0, 0);
            var pauseButton = game.add.button(0, 0, 'uiPauseButton', function () {
                F84.GameState.Instance.pause();
                _this.game.sound.play('button');
            }, _this);
            _this.add(pauseButton);
            pauseButton.alignIn(bounds, Phaser.TOP_RIGHT, 38, -22);
            var magnetGroup = _this.magnetGroup = game.add.group(_this);
            var magnetBG = game.add.sprite(0, 0, 'hudMagnetTimeBG', null, magnetGroup);
            var magnetText = _this.magnetText = game.add.text(0, 0, ':12', style, magnetGroup);
            magnetText.alignIn(magnetBG, Phaser.LEFT_CENTER, -95, 2);
            magnetGroup.alignIn(bounds, Phaser.BOTTOM_RIGHT, -7, -7);
            magnetGroup.alpha = 0;
            var studsBG = _this.studsBG = game.add.sprite(0, 0, 'hudCoinBG', null, _this);
            studsBG.alignIn(bounds, Phaser.BOTTOM_LEFT, -25, -21);
            var studsText = _this.studsText = game.add.text(0, 0, '720', style, _this);
            studsText.alignIn(studsBG, Phaser.LEFT_CENTER, -55, 4);
            var missileGroup = _this.missileGroup = game.add.group(_this);
            var missileBricksBG = game.add.sprite(0, 0, 'hudMissileCounter', null, missileGroup);
            var missileBricksText = _this.missileBricksText = game.add.text(0, 0, '0/' + F84.GameState.Instance.player.bossRequirement, style, missileGroup);
            missileBricksText.alignIn(missileBricksBG, Phaser.LEFT_CENTER, -60, 4);
            missileGroup.alignIn(bounds, Phaser.BOTTOM_LEFT, -26, -87);
            missileGroup.alpha = 0;
            _this.enemyAlert = game.add.group(_this);
            var glow = game.add.sprite(0, 0, 'tutorialWarningGlow', null, _this.enemyAlert);
            glow.anchor.set(0.5);
            var warningStyle = { fill: 'white', fontSize: 36, font: F84.Fonts.BoldFont, align: 'center', stroke: '#00000000', strokeThickness: 10 };
            var text = game.add.text(0, 0, F84.Strings.get('Warning'), warningStyle, _this);
            text.anchor.set(0.5);
            _this.enemyAlert.add(text);
            var icon1 = game.add.sprite(-180, 0, 'tutorialWarningIcon', null, _this.enemyAlert);
            icon1.anchor.set(0.5);
            var icon2 = game.add.sprite(180, 0, 'tutorialWarningIcon', null, _this.enemyAlert);
            icon2.anchor.set(0.5);
            _this.enemyAlert.alignIn(bounds, Phaser.CENTER);
            _this.enemyAlert.alpha = 0;
            _this.bossAlert = game.add.sprite(0, 0, 'JokerWarning', null, _this.enemyAlert);
            _this.bossAlert.anchor.set(0.5, 1);
            _this.bossAlert.alignTo(text, Phaser.TOP_CENTER, 0, 0);
            _this.bossAlert.alpha = 0;
            _this.fixedToCamera = true;
            return _this;
        }
        GameUI.prototype.update = function () {
            this.score.text = '' + F84.GameState.Instance.score;
            this.studsText.text = '' + F84.GameState.Instance.player.studs;
            if (this.magnetGroup.alpha == 1) {
                var magnetTimer = F84.GameState.Instance.player.magnetTimer;
                if (magnetTimer <= 0) {
                    this.magnetGroup.alpha = 0;
                    this.magnetEffect.pendingDestroy = true;
                    this.magnetEffect = null;
                }
                else
                    this.magnetText.text = F84.TimeFormat.format(Math.ceil(magnetTimer));
            }
        };
        GameUI.prototype.enableBossHPBar = function (boss) {
            boss.tookDamage.add(this.bossDamaged, this);
            this.createBossHealthBar(boss);
            if (this.brickNotification)
                this.brickNotification.dismiss();
            this.missileGroup.alpha = 1;
        };
        GameUI.prototype.createBossHealthBar = function (boss) {
            var maxHP = Math.min(Math.ceil(boss.maxHP), 6);
            var bossHP = this.bossHPGroup = this.game.add.group(this);
            var bossHPBG = this.game.add.sprite(0, 0, 'bossHealth' + maxHP, null, bossHP);
            var bossHPIcon = this.game.add.sprite(0, 0, boss.name + 'BossIcon', null, bossHP);
            bossHPBG.alignTo(bossHPIcon, Phaser.RIGHT_CENTER, -22, 0);
            for (var i = 0; i < maxHP; i++) {
                var hp = this.game.add.sprite(0, 0, 'bossHealthDot', null, bossHP);
                hp.alignIn(bossHPBG, Phaser.LEFT_CENTER, -24 - 21 * i, 0);
                this.bossHpDots.push(hp);
            }
            bossHP.alignTo(this.scoreBG, Phaser.BOTTOM_LEFT, 2, 2);
        };
        GameUI.prototype.disableBossHPBar = function () {
            this.bossHPGroup.destroy();
            this.missileGroup.alpha = 0;
        };
        GameUI.prototype.bossDamaged = function () {
            var hpDot = this.bossHpDots.pop();
            if (hpDot)
                hpDot.destroy();
        };
        GameUI.prototype.playerDamaged = function () {
            var _this = this;
            this.hpIcon.loadTexture('hudHealthIconHit');
            var timer = this.game.time.create();
            timer.add(600, function () { return _this.hpIcon.loadTexture('hudHealthIcon'); });
            timer.start();
            var hpDot = this.hpDots.pop();
            if (hpDot)
                hpDot.destroy();
        };
        GameUI.prototype.spawnCelebratoryMessage = function () {
            if (this.celebratoryMessage)
                this.celebratoryMessage.dismiss();
            this.celebratoryMessage = new F84.CelebratoryMessage(this.game);
            this.add(this.celebratoryMessage);
        };
        GameUI.prototype.spawnBrickNotification = function (powerup, count, max) {
            if (this.brickNotification)
                this.brickNotification.dismiss();
            this.brickNotification = new F84.BrickNotification(this.game, powerup, count, max, this.studsBG);
            this.add(this.brickNotification);
        };
        GameUI.prototype.updateMissileCounter = function (count, max) {
            var _this = this;
            this.missileBricksText.text = count + '/' + max;
            if (count >= max) {
                var timer = this.game.time.create();
                timer.add(500, function () {
                    if (F84.GameState.Instance.player.bossBricks == 0) {
                        _this.missileBricksText.text = '0/' + max;
                    }
                });
                timer.start();
            }
        };
        GameUI.prototype.activateMagnetUi = function () {
            this.magnetText.text = F84.TimeFormat.format(Math.ceil(F84.GameState.Instance.player.magnetTimer));
            this.magnetGroup.alpha = 1;
            if (this.magnetEffect == null) {
                this.magnetEffect = this.spawnMagnetEffect();
            }
        };
        GameUI.prototype.spawnMagnetEffect = function () {
            var _this = this;
            var player = F84.GameState.Instance.player;
            var group = this.game.add.group(F84.GameState.Instance.frontLayer);
            var _loop_1 = function (i) {
                var magnetfx = this_1.game.add.sprite(0, 0, 'magnetEffect', null, group);
                magnetfx.anchor.set(0.5);
                magnetfx.alpha = 0;
                var scale = this_1.game.add.tween(magnetfx.scale);
                var fadeTime = 300;
                this_1.game.add.tween(magnetfx).to({ alpha: 1 }, fadeTime, Phaser.Easing.Linear.None, true, 0 + (i * 300));
                scale.to({ x: 0, y: 0 }, 1000, Phaser.Easing.Linear.None, true, 0 + (i * 300));
                scale.onStart.addOnce(function () { scale.delay(0); });
                scale.onComplete.add(function () {
                    if (_this.magnetGroup.alpha != 0) {
                        magnetfx.scale.set(1);
                        magnetfx.alpha = 0;
                        if (!_this.magnetEffect.pendingDestroy) {
                            var newFade = _this.game.add.tween(magnetfx);
                            newFade.to({ alpha: 1 }, fadeTime, Phaser.Easing.Linear.None, true);
                            scale.start();
                        }
                    }
                });
            };
            var this_1 = this;
            for (var i = 0; i < 3; i++) {
                _loop_1(i);
            }
            group.alignIn(player, Phaser.CENTER);
            group.update = function () {
                group.position = player.sprite.position.clone();
            };
            return group;
        };
        GameUI.prototype.spawnMovementTutorial = function (onComplete, onCompleteContext) {
            var _this = this;
            var group = this.game.add.group(this);
            if (this.game.device.desktop) {
                var icon = this.game.add.sprite(0, -50, 'tutorialMoveWeb', null, group);
                icon.anchor.set(0.5);
                var glow = this.game.add.sprite(0, 100, 'uiHeaderGlow', null, group);
                glow.anchor.set(0.5);
                var style = { fill: 'white', fontSize: 36, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10, wordWrap: true, wordWrapWidth: 300, align: 'center' };
                var text = this.game.add.text(0, 100, '', style, group);
                text.setText(F84.Strings.get('MovementTutorialWeb'));
                text.lineSpacing = -18;
                text.anchor.set(0.5);
                group.alignIn(this.bounds, Phaser.CENTER);
            }
            else {
                var pointer = this.game.add.sprite(0, 0, 'tutorialMoveMobile', null, group);
                var glow = this.game.add.sprite(0, 300, 'uiHeaderGlow', null, group);
                glow.anchor.set(0.5);
                var style = { fill: 'white', fontSize: 36, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10, wordWrap: true, wordWrapWidth: 300, align: 'center' };
                var text = this.game.add.text(0, 300, F84.Strings.get('MovementTutorialMobile').toLocaleUpperCase(), style, group);
                text.lineSpacing = -18;
                text.anchor.set(0.5);
                group.alignIn(this.bounds, Phaser.CENTER);
                pointer.x -= 300;
                var tween = this.game.add.tween(pointer);
                tween.to({ x: pointer.x + 200 }, 1500, Phaser.Easing.Quadratic.Out, true, 0, -1);
            }
            F84.GameState.Instance.player.onMove.addOnce(function () {
                var tween = _this.game.add.tween(group);
                tween.to({ alpha: 0 }, 400, Phaser.Easing.Linear.None, true);
                tween.onComplete.add(function () {
                    group.destroy();
                    _this.spawnGameplayTutorials(onComplete, onCompleteContext);
                });
            });
        };
        GameUI.prototype.spawnGameplayTutorials = function (onComplete, onCompleteContext) {
            var _this = this;
            var add = function (name, next) { return _this.spawnGenericTutorial(name, next); };
            add('Collect', function () { return add('Obstacles', function () { return add('Enemies', function () { return onComplete.call(onCompleteContext); }); }); });
        };
        GameUI.prototype.spawnGenericTutorial = function (tutorial, onComplete) {
            var group = this.game.add.group(this);
            var icon = this.game.add.sprite(0, -80, 'tutorial' + tutorial, null, group);
            icon.anchor.set(0.5);
            var glow = this.game.add.sprite(0, 100, 'uiHeaderGlow', null, group);
            glow.anchor.set(0.5);
            var style = { fill: 'white', fontSize: 36, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10, wordWrap: true, wordWrapWidth: 400, align: 'center' };
            var text = this.game.add.text(0, 100, F84.Strings.get('Help' + tutorial).toLocaleUpperCase(), style, group);
            text.lineSpacing = -18;
            text.anchor.set(0.5);
            group.alignIn(this.bounds, Phaser.CENTER);
            var fadeIn = this.game.add.tween(group);
            fadeIn.from({ alpha: 0 }, 400, Phaser.Easing.Linear.None, true);
            var fadeOut = this.game.add.tween(group);
            fadeOut.to({ alpha: 0 }, 400, Phaser.Easing.Linear.None, false, 3500 - 400);
            fadeOut.onComplete.add(function () { return group.destroy(); });
            fadeIn.onComplete.add(function () { return fadeOut.start(); });
            if (onComplete)
                fadeOut.onComplete.add(onComplete);
        };
        GameUI.prototype.flashBossAlert = function (bossName) {
            var _this = this;
            this.bossAlert.loadTexture(bossName + 'Warning');
            this.bossAlert.alpha = 1;
            var tween = this.game.add.tween(this.enemyAlert);
            tween.to({ alpha: 1 }, 500, Phaser.Easing.Sinusoidal.InOut, true, 0, 4, true);
            tween.onComplete.add(function () { _this.enemyAlert.alpha = 0; _this.bossAlert.alpha = 0; });
            tween.onStart.add(function () { _this.game.sound.play('warning'); });
            var loops = 0;
            tween.onRepeat.add(function () {
                loops++;
                if (loops % 2 == 0) {
                    _this.game.sound.play('warning');
                }
            });
        };
        GameUI.prototype.flashEnemyAlert = function () {
            var _this = this;
            var tween = this.game.add.tween(this.enemyAlert);
            tween.to({ alpha: 1 }, 500, Phaser.Easing.Sinusoidal.InOut, true, 0, 4, true);
            tween.onComplete.add(function () { _this.enemyAlert.alpha = 0; });
            tween.onStart.add(function () { _this.game.sound.play('warning'); });
            var loops = 0;
            tween.onRepeat.add(function () {
                loops++;
                if (loops % 2 == 0) {
                    _this.game.sound.play('warning');
                }
            });
        };
        GameUI.prototype.spawnGameOverPopup = function () {
            this.spawnPopup('GameOver');
        };
        GameUI.prototype.spawnBossDefeatedPopup = function (bossName) {
            this.spawnPopup('BossDefeated' + bossName, 48);
        };
        GameUI.prototype.spawnPopup = function (string, fontSize) {
            if (fontSize === void 0) { fontSize = 80; }
            if (this.popup) {
                this.popup.dismiss();
            }
            var popup = this.popup = new F84.PopupMessage(this.game, string, fontSize);
            this.add(popup);
        };
        return GameUI;
    }(Phaser.Group));
    F84.GameUI = GameUI;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var HelpScreen = (function (_super) {
        __extends(HelpScreen, _super);
        function HelpScreen(game, onClosed, onClosedContext) {
            var _this = _super.call(this, game) || this;
            _this.onPauseMenuClosed = onClosed;
            _this.onPauseMenuClosedContext = onClosedContext;
            var overlay = F84.Overlay.create(game);
            _this.add(overlay);
            var popup = game.add.group(_this);
            var bg = game.add.sprite(0, 0, 'helpBG', null, popup);
            bg.alignIn(F84.GameState.Instance.bounds, Phaser.CENTER);
            var headerStyle = { fontSize: 32, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var headerText = game.add.text(0, 0, F84.Strings.get('Help'), headerStyle, popup);
            headerText.alignIn(bg, Phaser.TOP_CENTER, 0, 0);
            var style = { fontSize: 32, fill: F84.GameColors.WHITE, font: F84.Fonts.DefaultFont, wordWrap: true, wordWrapWidth: 250 };
            var tuts = game.add.group(popup);
            var moveGroup = game.add.group(tuts);
            var move = game.add.sprite(0, 0, 'helpMove' + (game.device.desktop ? 'Web' : 'Mobile'), null, moveGroup);
            var string = F84.Strings.get('MovementTutorial' + (game.device.desktop ? 'Web' : 'Mobile'));
            var moveText = game.add.text(0, 0, '', style, moveGroup);
            moveText.setText(string);
            moveText.text = moveText.text.substr(0, 1) + moveText.text.substring(1).toLocaleLowerCase();
            moveText.alignIn(move, Phaser.LEFT_CENTER, -250, 0);
            var collectGroup = game.add.group(tuts);
            var collect = game.add.sprite(0, 0, 'helpCollect', null, collectGroup);
            var collectText = game.add.text(0, 0, F84.Strings.get('HelpCollect'), style, collectGroup);
            collectText.alignIn(collect, Phaser.LEFT_CENTER, -250, 0);
            var obstaclesGroup = game.add.group(tuts);
            var obstacles = game.add.sprite(0, 0, 'helpObstacles', null, obstaclesGroup);
            var obstaclesText = game.add.text(0, 0, F84.Strings.get('HelpObstacles'), style, obstaclesGroup);
            obstaclesText.alignIn(obstacles, Phaser.LEFT_CENTER, -250, 0);
            var enemiesGroup = game.add.group(tuts);
            var enemies = game.add.sprite(0, 0, 'helpEnemies', null, enemiesGroup);
            var enemiesText = game.add.text(0, 0, F84.Strings.get('HelpEnemies'), style, enemiesGroup);
            enemiesText.alignIn(enemies, Phaser.LEFT_CENTER, -250, 0);
            tuts.align(1, 4, 0, 225);
            tuts.alignIn(bg, Phaser.CENTER);
            var continueButton = game.add.button(0, 0, 'helpExit', _this.continueButtonPressed, _this);
            _this.add(continueButton);
            continueButton.alignIn(bg, Phaser.TOP_RIGHT, 16, 12);
            _this.fixedToCamera = true;
            game.add.tween(popup).from({ alpha: 0 }, 500, Phaser.Easing.Quadratic.Out, true);
            game.add.tween(popup).from({ y: popup.y + 250 }, 750, Phaser.Easing.Quadratic.Out, true);
            return _this;
        }
        HelpScreen.prototype.continueButtonPressed = function () {
            this.game.sound.play('button');
            var menu = new F84.PauseMenu(this.game, this.onPauseMenuClosed, this.onPauseMenuClosedContext);
            this.game.add.existing(menu);
            this.destroy();
        };
        return HelpScreen;
    }(Phaser.Group));
    F84.HelpScreen = HelpScreen;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var JokerBoss = (function (_super) {
        __extends(JokerBoss, _super);
        function JokerBoss(game, player) {
            var _this = _super.call(this, game, player, 'bossJoker') || this;
            _this.name = 'Joker';
            _this.anchor.set(0.5);
            game.physics.enable(_this);
            _this.timer = 0;
            _this.movingY = false;
            _this.attacks = [_this.balloonAttack, _this.spreadAttack];
            _this.intro(function () { _this.movingY = true; });
            return _this;
        }
        JokerBoss.prototype.update = function () {
            var dt = F84.GameState.Instance.deltaTime;
            this.timer += dt;
            this.state();
        };
        JokerBoss.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            if (this.movingY)
                this.y = F84.GameState.Instance.camController.bounds.top + 100;
        };
        JokerBoss.prototype.balloonAttack = function () {
            var _this = this;
            var timer = 6;
            var balloonsRemaining = 5;
            var spawnTimer = 0;
            var spawnBalloon = function () {
                var balloon = new F84.Balloon(_this.game, _this.x, _this.y);
                _this.game.add.existing(balloon);
                F84.GameState.Instance.addObject(balloon);
                balloonsRemaining--;
                _this.game.sound.play('jokerBalloon');
            };
            return function () {
                spawnTimer -= F84.GameState.Instance.deltaTime;
                if (spawnTimer <= 0 && balloonsRemaining > 0) {
                    spawnBalloon();
                    spawnTimer += 0.8;
                }
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        JokerBoss.prototype.spreadAttack = function () {
            var _this = this;
            var timer = 6;
            var moveTime = 1.6;
            var moveTimer = moveTime;
            var dir = this.game.rnd.sign();
            var center = F84.GameState.Instance.bounds.centerX;
            var target = center + (F84.GameState.roadWidth / 2) * dir;
            var accel = 0.8;
            var bulletTime = 0.45;
            var bulletTimer = 0;
            return function () {
                _this.x -= (_this.x - target) * accel * F84.GameState.Instance.deltaTime;
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
                if (timer < 1) {
                    target = center;
                    accel = 2;
                    return;
                }
                var spawnBullet = function () {
                    var glow = _this.game.add.sprite(0, 0, 'jokerBulletGlow');
                    glow.anchor.set(0.5);
                    F84.GameState.Instance.addObject(glow);
                    glow.update = function () {
                        glow.position.set(bullet.x + Math.sin(bullet.angle) * 8, bullet.y + Math.cos(bullet.angle) * 8);
                        glow.angle += 180 * F84.GameState.Instance.deltaTime;
                    };
                    var sprite = _this.game.rnd.pick(['jokerBullet1', 'jokerBullet2', 'jokerBullet3']);
                    var bullet = new F84.Projectile(_this.game, _this.x, _this.y, sprite, 8, 8);
                    bullet.x += dir * 40;
                    bullet.body.velocity.y = 80;
                    _this.game.add.existing(bullet);
                    F84.GameState.Instance.addObject(bullet);
                    bullet.events.onDestroy.add(function () { return glow.destroy(); });
                    _this.game.sound.play('jokerRocket');
                };
                bulletTimer -= F84.GameState.Instance.deltaTime;
                if (bulletTimer <= 0) {
                    bulletTimer += bulletTime;
                    spawnBullet();
                }
                moveTimer -= F84.GameState.Instance.deltaTime;
                if (moveTimer <= 0) {
                    moveTimer += moveTime;
                    dir *= -1;
                    target = center + 180 * dir;
                }
            };
        };
        JokerBoss.prototype.finalAttack = function () {
            var _this = this;
            var timer = 6;
            var shakeTween = this.game.add.tween(this);
            shakeTween.to({ x: this.x + 5 }, 50, Phaser.Easing.Sinusoidal.InOut, true, 0, 10, true);
            var target = this.game.add.sprite(this.player.x, this.player.y, 'jokerTarget');
            target.anchor.set(0.5);
            F84.GameState.Instance.foreground.add(target);
            var targetOffset = new Phaser.Point(target.x - this.x, target.y - this.y);
            var gloveBack;
            var glove;
            var dist;
            var angle;
            return function () {
                var dt = F84.GameState.Instance.deltaTime;
                var prevTimer = timer;
                timer -= dt;
                if (timer <= 0)
                    _this.nextState();
                if (target) {
                    target.x = _this.x + targetOffset.x;
                    target.y = _this.y + targetOffset.y;
                }
                if (prevTimer >= 4.5 && timer < 4.5) {
                    target.destroy();
                    gloveBack = _this.game.add.sprite(0, 60, 'jokerGloveBack');
                    _this.addChild(gloveBack);
                    gloveBack.anchor.set(0.5, 0);
                    gloveBack.height = 1;
                    glove = new F84.Projectile(_this.game, 0, 0, 'jokerGlove', 40, 40);
                    F84.GameState.Instance.addObject(glove);
                    _this.addChild(glove);
                    glove.anchor.set(0.5, 0);
                    var gloveOrigin = new Phaser.Point(_this.x, _this.y + 60);
                    var targetPos = new Phaser.Point(_this.x + targetOffset.x, _this.y + targetOffset.y);
                    dist = gloveOrigin.distance(targetPos) - 60;
                    angle = -Math.atan2(targetPos.x - gloveOrigin.x, targetPos.y - gloveOrigin.y);
                    gloveBack.rotation = angle;
                    var heightTween = _this.game.add.tween(gloveBack);
                    heightTween.to({ height: dist }, 2500, Phaser.Easing.Elastic.Out, true);
                    var retractTween_1 = _this.game.add.tween(gloveBack);
                    retractTween_1.to({ height: -60 }, 800, Phaser.Easing.Quadratic.In);
                    heightTween.onComplete.add(function () { return retractTween_1.start(); });
                    retractTween_1.onComplete.add(function () {
                        if (glove)
                            glove.destroy();
                        if (gloveBack)
                            gloveBack.destroy();
                    });
                    _this.onDefeated.addOnce(function () {
                        if (glove)
                            glove.destroy();
                        if (gloveBack)
                            gloveBack.destroy();
                    });
                    _this.game.sound.play('jokerGlove');
                }
                if (glove) {
                    glove.position.set(gloveBack.x - Math.sin(gloveBack.rotation) * gloveBack.height, gloveBack.y + Math.cos(gloveBack.rotation) * gloveBack.height);
                    glove.rotation = angle;
                }
            };
        };
        JokerBoss.prototype.die = function () {
            var _this = this;
            var timer = 0;
            this.loadTexture('bossBrokenJoker');
            this.movingY = false;
            this.body.velocity.y = 0;
            this.body.enable = false;
            var spawnRepeatExplosions = this.repeatExplosionSpawner();
            this.spinOut();
            this.jettisonBoss(this.name);
            return function () {
                spawnRepeatExplosions();
                timer += F84.GameState.Instance.deltaTime;
                if (timer > 10)
                    _this.destroy();
            };
        };
        JokerBoss.environment = [F84.Environment.City, F84.Environment.Construction];
        return JokerBoss;
    }(F84.Boss));
    F84.JokerBoss = JokerBoss;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ObstacleHitbox = (function (_super) {
        __extends(ObstacleHitbox, _super);
        function ObstacleHitbox(game, x, y, width, height) {
            var _this = _super.call(this, game, x, y, 'whiteSquare') || this;
            _this.alpha = 0;
            game.physics.enable(_this);
            _this.body.setSize(width, height);
            return _this;
        }
        return ObstacleHitbox;
    }(Phaser.Sprite));
    F84.ObstacleHitbox = ObstacleHitbox;
    var Obstacle = (function (_super) {
        __extends(Obstacle, _super);
        function Obstacle(game, x, y, hitboxes, image) {
            var _this = _super.call(this, game, x, y, image) || this;
            var _loop_2 = function (hitbox) {
                var obj = new ObstacleHitbox(game, x + hitbox.x, y + hitbox.y, hitbox.width, hitbox.height);
                F84.GameState.Instance.addObject(obj);
                this_2.events.onDestroy.add(function () { return obj.destroy(); });
            };
            var this_2 = this;
            for (var _i = 0, hitboxes_1 = hitboxes; _i < hitboxes_1.length; _i++) {
                var hitbox = hitboxes_1[_i];
                _loop_2(hitbox);
            }
            if (image.includes('city')) {
                if (image.includes('obstacle_01')) {
                    var smoke = game.add.sprite(_this.width / 2, _this.height / 2 - 40, 'smoke1');
                    _this.addChild(smoke);
                    smoke.anchor.set(0.5);
                    smoke.animations.add('').play(10, true);
                    smoke.animations.currentAnim.frame = game.rnd.between(0, 10);
                }
                else if (image.includes('obstacle_02')) {
                    var angle = game.rnd.angle();
                    for (var i = 0; i < 2; i++) {
                        var light = game.add.sprite(_this.width / 2 + (i - 0.5) * 40, 150, image + '_light');
                        _this.addChild(light);
                        light.anchor.set(0.5);
                        light.angle = angle;
                        game.add.tween(light).to({ angle: light.angle + 360 }, 500, Phaser.Easing.Linear.None, true, 0, -1);
                    }
                }
                else if (image.includes('obstacle_03')) {
                    var smoke = game.add.sprite(_this.width / 2, _this.height / 2 - 80, 'smoke2');
                    _this.addChild(smoke);
                    smoke.anchor.set(0.5);
                    smoke.animations.add('').play(10, true);
                    smoke.animations.currentAnim.frame = game.rnd.between(0, 10);
                }
                else if (image.includes('obstacle_06')) {
                    var light = game.add.sprite(_this.width / 2, _this.height / 2, 'city/lbm_enviro_city_hazards_lights');
                    _this.addChild(light);
                    light.anchor.set(0.5);
                    game.add.tween(light).to({ alpha: 0 }, 500, Phaser.Easing.Linear.None, true, 0, -1, true);
                }
            }
            else if (image.includes('const')) {
                if (image.includes('obstacle_02')) {
                    var numLights = 4;
                    var offset = (_this.width - 5) / (numLights - 1);
                    var frame = game.rnd.between(0, 14);
                    var _loop_3 = function (i) {
                        var light = game.add.sprite(0, 0, 'warningLight');
                        F84.GameState.Instance.frontLayer.add(light);
                        light.alignIn(this_3, Phaser.TOP_RIGHT, 16 - (offset * i), 16);
                        light.animations.add('').play(16, true);
                        light.animations.currentAnim.frame = frame;
                        this_3.events.onDestroy.add(function () { return light.destroy(); });
                    };
                    var this_3 = this;
                    for (var i = 0; i < numLights; i++) {
                        _loop_3(i);
                    }
                }
                else if (image.includes('obstacle_03')) {
                    var numLights = 2;
                    var offset = (_this.width - 33) / (numLights - 1);
                    var frame = game.rnd.between(0, 14);
                    var _loop_4 = function (i) {
                        var light = game.add.sprite(0, 0, 'warningLight');
                        F84.GameState.Instance.frontLayer.add(light);
                        light.alignIn(this_4, Phaser.BOTTOM_RIGHT, 15 - (offset * i), 5);
                        light.animations.add('').play(16, true);
                        light.animations.currentAnim.frame = frame;
                        this_4.events.onDestroy.add(function () { return light.destroy(); });
                    };
                    var this_4 = this;
                    for (var i = 0; i < numLights; i++) {
                        _loop_4(i);
                    }
                }
                else if (image.includes('obstacle_06')) {
                    var numLights = 3;
                    var offset = (_this.width - 12) / (numLights - 1);
                    var frame = game.rnd.between(0, 14);
                    var _loop_5 = function (i) {
                        var light = game.add.sprite(0, 0, 'warningLight');
                        F84.GameState.Instance.frontLayer.add(light);
                        light.alignIn(this_5, Phaser.TOP_LEFT, 16 - (offset * i), 16);
                        light.animations.add('').play(16, true);
                        light.animations.currentAnim.frame = frame;
                        this_5.events.onDestroy.add(function () { return light.destroy(); });
                    };
                    var this_5 = this;
                    for (var i = 0; i < numLights; i++) {
                        _loop_5(i);
                    }
                }
            }
            return _this;
        }
        return Obstacle;
    }(Phaser.Sprite));
    F84.Obstacle = Obstacle;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PauseMenu = (function (_super) {
        __extends(PauseMenu, _super);
        function PauseMenu(game, onClosed, onClosedContext) {
            var _this = _super.call(this, game) || this;
            var bounds = F84.GameState.Instance.bounds;
            var overlay = F84.Overlay.create(game, 0.9);
            _this.add(overlay);
            var image = game.add.sprite(0, 0, 'pauseBatwing', null, _this);
            image.alignIn(bounds, Phaser.CENTER, 0, 300);
            var logoLego = game.add.sprite(0, 0, 'uiLogoLegoBatman', null, _this);
            logoLego.alignIn(bounds, Phaser.TOP_CENTER, 0, -10);
            var textBG = game.add.sprite(0, 0, 'uiHeaderGlow', null, _this);
            textBG.alignIn(bounds, Phaser.TOP_CENTER, 0, -100);
            var style = { fontSize: 48, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 10 };
            var text = game.add.text(0, 0, F84.Strings.get('PauseHeader'), style, _this);
            text.alignIn(textBG, Phaser.CENTER);
            var quitButton = game.add.button(0, 0, 'uiPauseHomeButton', _this.quitButtonPressed, _this);
            _this.add(quitButton);
            quitButton.alignIn(bounds, Phaser.TOP_CENTER, -150 + 40, -252);
            var helpButton = game.add.button(0, 0, 'uiPauseHelpButton', _this.helpButtonPressed, _this);
            _this.add(helpButton);
            helpButton.alignIn(bounds, Phaser.TOP_CENTER, 0 + 40, -250);
            var volumeButton = F84.UIFactory.createLargeSoundButton(game, _this);
            volumeButton.alignIn(bounds, Phaser.TOP_CENTER, 150 + 40, -254);
            var continueButton = game.add.button(0, 0, 'uiPlayButton', _this.continueButtonPressed, _this);
            _this.add(continueButton);
            continueButton.alignIn(bounds, Phaser.BOTTOM_CENTER, 78 / 2, 30);
            var continueStyle = { fontSize: 32, fill: F84.GameColors.WHITE, font: F84.Fonts.BoldFont, stroke: '#00000000', strokeThickness: 8 };
            var continueText = game.add.text(0, 0, F84.Strings.get('Resume'), continueStyle, _this);
            continueText.alignTo(continueButton, Phaser.TOP_CENTER, -78 / 2, 0);
            style = { fontSize: 20, font: 'Arial', fontWeight: 'bold', fill: F84.GameColors.WHITE, align: 'center' };
            var policyGroup = new Phaser.Group(game, _this);
            var legalString = F84.Strings.get('DCLegalLines');
            if (F84.LegoUtils.onLegoSite()) {
                var separator = new Phaser.Text(game, 0, 0, ' | ', style);
                policyGroup.addChild(separator);
                var privacyPolicy = new Phaser.Text(game, 0, 0, '', style);
                privacyPolicy.setText(F84.Strings.get('CookiePolicy'));
                policyGroup.addChild(privacyPolicy);
                privacyPolicy.inputEnabled = true;
                privacyPolicy.events.onInputOver.add(function () { return document.body.style.cursor = 'pointer'; }, _this);
                privacyPolicy.events.onInputOut.add(function () { return document.body.style.cursor = 'default'; }, _this);
                privacyPolicy.events.onInputUp.add(function () { return window.open(F84.GameConfig.get('privacyLink')); }, _this);
                privacyPolicy.alignTo(separator, Phaser.LEFT_CENTER);
                var cookiePolicy = new Phaser.Text(game, 0, 0, '', style);
                cookiePolicy.setText(F84.Strings.get('CookiePolicy'));
                policyGroup.addChild(cookiePolicy);
                cookiePolicy.inputEnabled = true;
                cookiePolicy.events.onInputOver.add(function () { return document.body.style.cursor = 'pointer'; }, _this);
                cookiePolicy.events.onInputOut.add(function () { return document.body.style.cursor = 'default'; }, _this);
                cookiePolicy.events.onInputUp.add(function () { return window.open(F84.GameConfig.get('cookieLink')); }, _this);
                cookiePolicy.alignTo(separator, Phaser.RIGHT_CENTER);
                legalString += '\n' + F84.Strings.get('LegoLegalLines');
            }
            style.fontSize = 16;
            style.fontWeight = '';
            var legalLines = new Phaser.Text(game, 0, 0, legalString, style);
            legalLines.lineSpacing = -5;
            policyGroup.alignIn(continueButton, Phaser.BOTTOM_CENTER, -72 / 2, -72 / 2);
            _this.onClosed = onClosed;
            _this.onClosedContext = onClosedContext;
            _this.fixedToCamera = true;
            return _this;
        }
        PauseMenu.prototype.quitButtonPressed = function () {
            var _this = this;
            this.game.sound.play('button');
            var config = {
                messageText: F84.Strings.get('ConfirmQuit'),
                yesButtonText: F84.Strings.get('ConfirmQuitYes'),
                noButtonText: F84.Strings.get('ConfirmQuitNo'),
                yesFunction: function () {
                    F84.Game.Instance.state.start('MainMenu');
                    _this.onClosed.call(_this.onClosedContext);
                },
                noFunction: this.onClosed,
                noContext: this.onClosedContext,
                fixedToCamera: true
            };
            var yesNoDialogueGroup = new F84.YesNoDialogueGroup(this.game, config);
            this.game.add.existing(yesNoDialogueGroup);
            this.destroy();
        };
        PauseMenu.prototype.helpButtonPressed = function () {
            this.game.sound.play('button');
            var prompt = new F84.HelpScreen(this.game, this.onClosed, this.onClosedContext);
            this.game.add.existing(prompt);
            this.destroy();
        };
        PauseMenu.prototype.continueButtonPressed = function () {
            this.game.sound.play('button');
            this.destroy();
            this.onClosed.call(this.onClosedContext);
        };
        return PauseMenu;
    }(Phaser.Group));
    F84.PauseMenu = PauseMenu;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Player = (function (_super) {
        __extends(Player, _super);
        function Player(game) {
            var _this = _super.call(this, game, 300, 300, 'whiteSquare') || this;
            _this.invulnTime = 1;
            _this.hitTime = 1;
            _this.magnetTime = 16;
            _this.boostTime = 8;
            _this.width = 38;
            _this.height = 62;
            _this.anchor.set(0.5);
            _this.tint = 0xDD2222AA;
            _this.alpha = 0;
            game.physics.enable(_this);
            _this.vehicle = 'batmobile';
            _this.transformThreshold = null;
            var sprite = _this.sprite = game.add.sprite(0, 0, _this.vehicle);
            F84.GameState.Instance.playerGroup.add(sprite);
            sprite.anchor.set(0.5, 134 / 204);
            sprite.postUpdate = function () { return sprite.position.set(_this.body.x + _this.body.halfWidth, _this.body.y + _this.body.halfHeight); };
            _this.addTrail();
            _this.setStreak();
            _this.invuln = 0;
            _this.invulnFade = true;
            _this.hitTimer = 0;
            _this.baseSpeed = F84.GameConfig.get('startingMovementSpeed');
            _this.speed = _this.baseSpeed;
            _this.body.velocity.y = -_this.speed;
            _this.isInputEnabled = true;
            var upgradeData = _this.game.cache.getJSON('upgrades');
            var upgrades = F84.PlayerData.Instance.saveData.upgrades;
            var getUpgrade = function (key) {
                var i = upgrades[key];
                return upgradeData[key][i];
            };
            _this.magnetBricks = 0;
            _this.magnetTimer = 0;
            _this.magnetRequirement = getUpgrade('magnet');
            _this.magnetRange = 500;
            _this.collectionRange = 70;
            _this.boostBricks = 0;
            _this.boostTimer = 0;
            _this.boostRequirement = getUpgrade('rocketBooster');
            _this.bossBricks = 0;
            _this.bossRequirement = getUpgrade('homingRocket');
            _this.studs = 0;
            _this.studCombo = 0;
            _this.studValue = getUpgrade('stud');
            _this.maxHP = getUpgrade('health');
            _this.hp = _this.maxHP;
            _this.onMove = new Phaser.Signal();
            _this.onLose = new Phaser.Signal();
            _this.tookDamage = new Phaser.Signal();
            return _this;
        }
        Player.prototype.addTrail = function () {
            var trail = this.trail = this.game.add.sprite(0, 50, 'vehicleTrail');
            trail.anchor.set(18 / 45, 15 / 80);
            trail.animations.add('').play(24, true);
            this.sprite.addChild(trail);
        };
        Player.prototype.setStreak = function () {
            if (!this.streak) {
                this.streak = this.game.add.sprite(0, 0, 'airStreak1');
                this.sprite.addChild(this.streak);
            }
            if (this.vehicle == 'batmobile') {
                this.streak.loadTexture('airStreak2');
                this.streak.anchor.set(0.51, 0.32);
                this.streak.animations.add('').play(16, true);
            }
            else {
                this.streak.loadTexture('airStreak1');
                this.streak.anchor.set(0.5, 0.32);
                this.streak.animations.add('').play(16, true);
            }
        };
        Player.prototype.setSprite = function (vehicle, direction) {
            var sprite = vehicle;
            if (this.hitTimer > 0 || this.hp <= 0)
                sprite += 'Hit';
            else {
                if (direction == -1)
                    sprite += 'Left';
                if (direction == 1)
                    sprite += 'Right';
            }
            if (this.sprite.key != sprite)
                this.sprite.loadTexture(sprite);
        };
        Player.prototype.hide = function () {
            this.sprite.visible = false;
            if (this.boostTimer > 0)
                this.boostEffect.finish();
            if (this.magnetTimer > 0)
                F84.GameState.Instance.ui.magnetEffect.alpha = 0;
            if (this.trail)
                this.trail.visible = false;
            this.invuln = 99999;
            this.isInputEnabled = false;
            this.body.velocity.x = 0;
        };
        Player.prototype.unhide = function () {
            this.sprite.visible = true;
            this.sprite.alpha = 1;
            if (this.boostTimer > 0) {
                this.boostEffect = new F84.BoostEffect(this.game, this, this.vehicle);
                this.game.add.existing(this.boostEffect);
            }
            if (this.magnetTimer > 0)
                F84.GameState.Instance.ui.magnetEffect.alpha = 1;
            if (this.trail)
                this.trail.visible = true;
            this.invuln = 1.5;
            this.invulnFade = false;
            this.isInputEnabled = true;
        };
        Player.prototype.transform = function () {
            if (this.vehicle == 'batmobile')
                this.transformSky();
            else
                this.transformRoad();
            F84.GameState.Instance.playTransformationAnimation(this.vehicle);
            this.setStreak();
        };
        Player.prototype.transformRoad = function () {
            this.vehicle = 'batmobile';
            this.setSprite(this.vehicle, 0);
            this.sprite.anchor.set(0.5, 134 / 204);
            this.addTrail();
        };
        Player.prototype.transformSky = function () {
            this.vehicle = 'batwing';
            this.setSprite(this.vehicle, 0);
            this.trail.destroy();
            this.sprite.anchor.set(0.5, 0.5);
        };
        Player.prototype.update = function () {
            var _this = this;
            var dt = F84.GameState.Instance.deltaTime;
            var keyDown = function (key) { return _this.game.input.keyboard.isDown(key); };
            var bounds = F84.GameState.Instance.bounds;
            var leftBound = bounds.centerX - F84.GameState.roadWidth / 2 + 10;
            var rightBound = bounds.centerX + F84.GameState.roadWidth / 2 - 10;
            this.body.velocity.y -= (this.body.velocity.y + this.speed) * 5 * dt;
            var dir = 0;
            if (this.isInputEnabled) {
                if (keyDown(Phaser.KeyCode.LEFT))
                    dir--;
                if (keyDown(Phaser.KeyCode.RIGHT))
                    dir++;
                if (dir != 0)
                    this.onMove.dispatch();
                var pointer = this.game.input.pointer1;
                if (pointer.isDown) {
                    var target = pointer.x;
                    var diff = target - (this.body.x + this.body.halfWidth);
                    dir = (diff > 0) ? 1 : -1;
                    if (Math.abs(diff) < 15)
                        dir = 0;
                    this.body.velocity.x = -((this.body.x + this.body.width / 2) - target) * 5;
                    this.onMove.dispatch();
                }
            }
            this.body.velocity.x -= (this.body.velocity.x - 520 * dir) * 5 * dt;
            this.setSprite(this.vehicle, dir);
            if (this.body.x < leftBound)
                this.body.velocity.x -= (this.body.x - leftBound) * 300 * dt;
            if (this.body.x + this.width > rightBound)
                this.body.velocity.x -= ((this.body.x + this.width) - rightBound) * 300 * dt;
            var targetRotation = this.body.velocity.x * 0.0008;
            this.sprite.rotation -= (this.sprite.rotation - targetRotation) * 12 * dt;
            this.hitTimer -= dt;
            this.invuln -= dt;
            if (this.invuln > 0) {
                if (this.invulnFade) {
                    var on = Math.floor((this.invuln / this.invulnTime) * 8) % 2;
                    this.sprite.alpha = on ? 1 : 0;
                    if (this.boostEffect != null)
                        this.boostEffect.setAlpha(on ? 1 : 0);
                }
            }
            else {
                if (this.boostEffect != null)
                    this.boostEffect.setAlpha(1);
                this.sprite.alpha = 1;
                this.invulnFade = true;
            }
            if (this.boostTimer > 0) {
                this.boostTimer -= dt;
                if (this.boostTimer <= 1.5) {
                    this.invuln = this.boostTimer + 1;
                    this.invulnFade = false;
                }
                if (this.boostTimer <= 0) {
                    this.speed = this.baseSpeed;
                    this.boostEffect.finish();
                }
            }
            var range = this.collectionRange;
            if (this.magnetTimer > 0) {
                this.magnetTimer -= dt;
                range = this.magnetRange;
            }
            for (var _i = 0, _a = F84.GameState.Instance.objects; _i < _a.length; _i++) {
                var obj = _a[_i];
                if (obj instanceof F84.Stud && obj.body != null) {
                    var point = this.body.position;
                    if (!this.isInputEnabled) {
                        point = this.transformation.body.position;
                    }
                    var dx = point.x - obj.body.x;
                    var dy = point.y - obj.body.y;
                    var dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist <= range)
                        obj.magnetize();
                }
            }
            if (this.transformThreshold && this.body.y < this.transformThreshold) {
                this.transformThreshold = null;
                this.transform();
            }
        };
        Player.prototype.onHitObject = function (us, other) {
            if (!this.isInputEnabled)
                return;
            if ((other instanceof F84.ObstacleHitbox || other instanceof F84.Projectile || other instanceof F84.Enemy) && this.invuln <= 0 && this.boostTimer <= 0) {
                this.takeDamage();
                var key = 'hitEffect';
                if (other instanceof F84.Enemy)
                    key += F84.GameState.Instance.upcomingBoss;
                if (other instanceof F84.Projectile)
                    key += F84.GameState.Instance.currentBoss;
                var hit = this.game.add.sprite(this.body.center.x, this.body.center.y, key);
                F84.GameState.Instance.addObject(hit, F84.GameState.Instance.foreground);
                hit.anchor.set(58 / 143, 70 / 134);
                var angle = Math.atan2(this.body.center.x - other.centerX, other.centerY - this.body.center.y);
                angle += Math.PI / 2;
                hit.position.add(Math.cos(angle) * 50, Math.sin(angle) * 50);
                hit.animations.add('').play(20, false, true);
            }
            if (other instanceof F84.Stud) {
                var value = this.studValue;
                if (value - Math.floor(value) == 0.5) {
                    value = (this.studs % 2 == 0) ? 2 : 1;
                }
                this.studs += value;
                F84.GameState.Instance.score += value;
                other.collect();
                this.game.sound.play('studCollect');
                this.studCombo++;
                if (this.studCombo >= 50) {
                    F84.GameState.Instance.ui.spawnCelebratoryMessage();
                    this.studCombo = 0;
                }
            }
            if (other instanceof F84.Powerup) {
                other.loadTexture('powerUpCollection');
                var anim = other.animations.add('play').play(60);
                anim.onComplete.addOnce(this.collectPowerUP, this, 0, other);
                F84.GameState.Instance.world.bringToTop(other);
                other.body.enable = false;
                var type = other.powerupType;
                if (type == 'magnet')
                    this.game.sound.play('magnetCollect');
                else if (type == 'boost')
                    this.game.sound.play('boosterCollect');
                else if (type == 'bossWeapon')
                    this.game.sound.play('rocketCollect');
            }
        };
        Player.prototype.collectPowerUP = function (other) {
            F84.GameState.Instance.destroyObject(other);
            var type = other.powerupType;
            if (type == 'magnet')
                this.collectMagnetBrick();
            else if (type == 'boost')
                this.collectBoostBrick();
            else if (type == 'bossWeapon')
                this.collectBossWeapon();
        };
        Player.prototype.takeDamage = function () {
            this.body.velocity.y = 300;
            this.invuln = this.invulnTime;
            this.hitTimer = this.hitTime;
            this.hp--;
            this.tookDamage.dispatch();
            if (this.hp <= 0) {
                this.onLose.dispatch();
                this.speed = 0;
            }
            this.game.sound.play('impact');
        };
        Player.prototype.collectMagnetBrick = function () {
            this.magnetBricks++;
            F84.GameState.Instance.ui.spawnBrickNotification('Magnet', this.magnetBricks, this.magnetRequirement);
            if (this.magnetBricks >= this.magnetRequirement) {
                this.magnetBricks = 0;
                this.magnetTimer = this.magnetTime;
                this.playPowerupAnimation('magnet');
                F84.GameState.Instance.ui.activateMagnetUi();
            }
        };
        Player.prototype.collectBoostBrick = function () {
            this.boostBricks++;
            F84.GameState.Instance.ui.spawnBrickNotification('Boost', this.boostBricks, this.boostRequirement);
            if (this.boostBricks >= this.boostRequirement) {
                this.boostBricks = 0;
                this.boostTimer = this.boostTime;
                this.speed = this.baseSpeed * 2;
                this.playPowerupAnimation('boost');
                if (this.boostEffect == null) {
                    this.boostEffect = new F84.BoostEffect(this.game, this, this.vehicle);
                    this.game.add.existing(this.boostEffect);
                }
            }
        };
        Player.prototype.collectBossWeapon = function () {
            var _this = this;
            this.bossBricks++;
            F84.GameState.Instance.ui.updateMissileCounter(this.bossBricks, this.bossRequirement);
            if (this.bossBricks >= this.bossRequirement) {
                this.bossBricks = 0;
                this.playPowerupAnimation('bossWeapon', function () {
                    var missile = _this.game.add.existing(new F84.PlayerMissile(_this.game, _this.x, _this.y));
                    F84.GameState.Instance.addObject(missile);
                });
            }
        };
        Player.prototype.playPowerupAnimation = function (powerup, onComplete, onCompleteContext) {
            var anim = new F84.PowerupAnimation(this.game, powerup, onComplete, onCompleteContext);
            F84.GameState.Instance.foreground.add(anim);
        };
        return Player;
    }(Phaser.Sprite));
    F84.Player = Player;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PlayerMissile = (function (_super) {
        __extends(PlayerMissile, _super);
        function PlayerMissile(game, x, y) {
            var _this = _super.call(this, game, x, y, 'rocket') || this;
            _this.anchor.set(0.5);
            _this.target = F84.GameState.Instance.objects.find(function (o) { return o instanceof F84.Boss; });
            game.physics.enable(_this);
            _this.body.velocity.y = -500;
            var trail = game.add.sprite(0, 16, 'rocketTrail');
            trail.anchor.set(28 / 57, 4 / 79);
            trail.animations.add('').play(16, true);
            _this.addChild(trail);
            game.sound.play('rocketLaunch');
            return _this;
        }
        PlayerMissile.prototype.update = function () {
            var dt = F84.GameState.Instance.deltaTime;
            this.target = F84.GameState.Instance.objects.find(function (o) { return o instanceof F84.Boss; });
            if (this.target) {
                var dx = this.target.x - this.x;
                var dy = this.target.y - this.y;
                var angle = Math.atan2(dy, dx);
                this.body.x += Math.cos(angle) * 400 * dt;
                this.body.y += Math.sin(angle) * 400 * dt;
                this.body.velocity.x += Math.cos(angle) * 1000 * dt;
                this.body.velocity.y += Math.sin(angle) * 1000 * dt;
            }
            var vel = new Phaser.Point(this.body.velocity.x, this.body.velocity.y);
            this.rotation = Math.atan2(vel.y, vel.x) + Math.PI / 2;
        };
        return PlayerMissile;
    }(Phaser.Sprite));
    F84.PlayerMissile = PlayerMissile;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PopupMessage = (function (_super) {
        __extends(PopupMessage, _super);
        function PopupMessage(game, message, fontSize) {
            var _this = _super.call(this, game) || this;
            var redBlur = _this.game.add.sprite(0, 0, 'hudRedBlur', null, _this);
            redBlur.anchor.set(0.5);
            var style = { fill: 'white', fontSize: fontSize, font: F84.Fonts.BoldFont, align: "center" };
            var text = _this.game.add.text(0, 0, F84.Strings.get(message), style, _this);
            text.stroke = '#00000000';
            text.strokeThickness = 50;
            text.alignIn(redBlur, Phaser.CENTER);
            _this.alignIn(F84.GameState.Instance.ui.bounds, Phaser.CENTER);
            var tweenIn = _this.game.add.tween(_this.scale);
            tweenIn.from({ x: 0.02, y: 0.02 }, 800, Phaser.Easing.Back.Out, true);
            var timer = _this.game.time.create(true);
            timer.add(2200, _this.dismiss, _this);
            tweenIn.onComplete.add(function () {
                timer.start();
            });
            return _this;
        }
        PopupMessage.prototype.dismiss = function () {
            var _this = this;
            if (this.dismissing)
                return;
            this.dismissing = true;
            var tweenOut = this.game.add.tween(this.scale);
            tweenOut.to({ x: 0.02, y: 0.02 }, 800, Phaser.Easing.Quadratic.In, true);
            tweenOut.onComplete.add(function () { return _this.destroy(); });
        };
        return PopupMessage;
    }(F84.Popup));
    F84.PopupMessage = PopupMessage;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Powerup = (function (_super) {
        __extends(Powerup, _super);
        function Powerup(game, x, y, powerupType) {
            var _this = _super.call(this, game, x, y, powerupType + 'Brick') || this;
            _this.powerupType = powerupType;
            _this.anchor.set(0.5);
            _this.animations.add('').play(24, true);
            game.physics.enable(_this);
            _this.body.setSize(60, 60, _this.width / 2 - 30, _this.width / 2 - 30);
            return _this;
        }
        return Powerup;
    }(Phaser.Sprite));
    F84.Powerup = Powerup;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PowerupAnimation = (function (_super) {
        __extends(PowerupAnimation, _super);
        function PowerupAnimation(game, powerup, onComplete, onCompleteContext) {
            var _this = _super.call(this, game) || this;
            _this.spawnTime = 0.1;
            _this.powerup = powerup;
            _this.onComplete = onComplete;
            _this.onCompleteContext = onCompleteContext;
            var player = F84.GameState.Instance.player;
            var glow = _this.glow = game.add.sprite(player.x, player.y, powerup + 'Glow');
            F84.GameState.Instance.frontLayer.add(glow);
            glow.anchor.set(0.5);
            glow.postUpdate = function () {
                glow.position.set(player.x, player.y - 10);
                glow.angle += 180 * F84.GameState.Instance.deltaTime;
            };
            var scaleTween = game.add.tween(glow.scale);
            scaleTween.from({ x: 0.5, y: 0.5 }, 500, Phaser.Easing.Back.Out, true);
            var fadeTween = game.add.tween(glow);
            fadeTween.from({ alpha: 0 }, 400, Phaser.Easing.Linear.None, true);
            _this.spawnTimer = _this.spawnTime;
            _this.timer = 1.6;
            game.sound.play('powerUpBricks');
            return _this;
        }
        Object.defineProperty(PowerupAnimation.prototype, "localPlayerPos", {
            get: function () {
                var player = F84.GameState.Instance.player;
                return new Phaser.Point(player.x - this.x, player.y - this.y);
            },
            enumerable: true,
            configurable: true
        });
        PowerupAnimation.prototype.spawnPiece = function () {
            var _this = this;
            var pos = this.localPlayerPos;
            var key = this.powerup + 'Piece' + this.game.rnd.between(1, 4);
            var ang = this.game.rnd.frac() * Math.PI * 2;
            var piece = this.game.add.sprite(pos.x + Math.cos(ang) * 900, pos.y + Math.sin(ang) * 900, key, null, this);
            piece.anchor.set(0.5);
            piece.scale.set(0.5);
            piece.update = function () {
                var pos = _this.localPlayerPos;
                piece.x -= (piece.x - pos.x) * 10 * F84.GameState.Instance.deltaTime;
                piece.y -= (piece.y - pos.y) * 10 * F84.GameState.Instance.deltaTime;
            };
            var tweenIn = this.game.add.tween(piece.scale);
            tweenIn.to({ x: 1.75, y: 1.75 }, 150, Phaser.Easing.Quadratic.Out, true);
            var tweenOut = this.game.add.tween(piece.scale);
            tweenOut.to({ x: 0.15, y: 0.15 }, 250, Phaser.Easing.Quadratic.In);
            tweenIn.onComplete.add(function () { return tweenOut.start(); });
            var fadeOut = this.game.add.tween(piece);
            fadeOut.to({ alpha: 0 }, 50, Phaser.Easing.Linear.None, false, 200);
            tweenIn.onComplete.add(function () { return fadeOut.start(); });
            fadeOut.onComplete.add(function () { return piece.destroy(); });
        };
        PowerupAnimation.prototype.update = function () {
            var _this = this;
            _super.prototype.update.call(this);
            var prevTimer = this.timer;
            this.timer -= F84.GameState.Instance.deltaTime;
            if (this.timer > 0.5) {
                this.spawnTimer -= F84.GameState.Instance.deltaTime;
                if (this.spawnTimer <= 0) {
                    this.spawnPiece();
                    this.spawnTimer += this.spawnTime;
                }
            }
            if (prevTimer > 0.5 && this.timer <= 0.5) {
                if (this.onComplete)
                    this.onComplete.call(this.onCompleteContext);
                var fadeTween = this.game.add.tween(this.glow);
                fadeTween.to({ alpha: 0 }, 500, Phaser.Easing.Linear.None, true);
                fadeTween.onComplete.add(function () { return _this.destroy(); });
            }
        };
        PowerupAnimation.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            this.position.set(this.game.camera.x, this.game.camera.y);
        };
        return PowerupAnimation;
    }(Phaser.Group));
    F84.PowerupAnimation = PowerupAnimation;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var RiddlerBoss = (function (_super) {
        __extends(RiddlerBoss, _super);
        function RiddlerBoss(game, player) {
            var _this = _super.call(this, game, player, 'bossRiddler') || this;
            _this.name = 'Riddler';
            _this.anchor.set(0.5);
            game.physics.enable(_this);
            _this.timer = 0;
            _this.movingY = false;
            var blades = game.add.sprite(0, -_this.height / 2, 'riddlerBlades');
            blades.animations.add('').play(24, true);
            blades.anchor.set(0.5);
            _this.addChild(blades);
            _this.attacks = [_this.hatAttack, _this.spreadAttack];
            _this.intro(function () { _this.movingY = true; });
            return _this;
        }
        RiddlerBoss.prototype.update = function () {
            var dt = F84.GameState.Instance.deltaTime;
            this.timer += dt;
            this.x = F84.GameState.Instance.bounds.centerX + 100 * Math.sin(this.timer);
            this.state();
        };
        RiddlerBoss.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            if (this.movingY)
                this.y = F84.GameState.Instance.camController.bounds.top + 100;
        };
        RiddlerBoss.prototype.hatAttack = function () {
            var _this = this;
            var timer = 5;
            var hatsRemaining = 3;
            var spawnTimer = 0;
            var spawnHat = function () {
                var hat = new F84.BouncingBullet(_this.game, _this.x, _this.y);
                _this.game.add.existing(hat);
                F84.GameState.Instance.addObject(hat);
                hatsRemaining--;
                _this.game.sound.play('riddlerHat', 0.7);
            };
            return function () {
                spawnTimer -= F84.GameState.Instance.deltaTime;
                if (spawnTimer <= 0 && hatsRemaining > 0) {
                    spawnHat();
                    spawnTimer += 1.2;
                }
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        RiddlerBoss.prototype.spreadAttack = function () {
            var _this = this;
            var timer = 5;
            var bulletsRemaining = 4;
            var waveTimer = 1.5;
            var wavesRemaining = 1;
            var spawnTimer = 0;
            var offset = 0.3 * this.game.rnd.sign();
            var spawnBullet = function () {
                var angle = _this.angleToPlayer() + (offset / 4) * (bulletsRemaining - 2);
                var glow = _this.game.add.sprite(0, 0, 'riddlerBulletGlow');
                glow.anchor.set(0.5);
                F84.GameState.Instance.addObject(glow);
                glow.update = function () {
                    glow.position.set(bullet.x + Math.cos(angle) * 32, bullet.y + Math.sin(angle) * 32);
                    glow.angle += 180 * F84.GameState.Instance.deltaTime;
                };
                var bullet = new F84.Projectile(_this.game, _this.x, _this.y, 'riddlerBullet', 20, 20);
                bullet.rotateWithMovement = true;
                bullet.body.velocity.set(Math.cos(angle) * 350, Math.sin(angle) * 350);
                bullet.body.velocity.multiply(0.5, 0.5);
                _this.game.add.existing(bullet);
                F84.GameState.Instance.addObject(bullet);
                bullet.events.onDestroy.add(function () { return glow.destroy(); });
                bulletsRemaining--;
                _this.game.sound.play('riddlerRocket', 0.7);
            };
            return function () {
                waveTimer -= F84.GameState.Instance.deltaTime;
                if (waveTimer <= 0 && wavesRemaining > 0) {
                    waveTimer += 0.5;
                    spawnTimer = 0;
                    bulletsRemaining = 4;
                    wavesRemaining--;
                }
                spawnTimer -= F84.GameState.Instance.deltaTime;
                if (spawnTimer <= 0 && bulletsRemaining > 0) {
                    spawnBullet();
                    spawnTimer += 0.2;
                }
                timer -= F84.GameState.Instance.deltaTime;
                if (timer <= 0)
                    _this.nextState();
            };
        };
        RiddlerBoss.prototype.finalAttack = function () {
            var _this = this;
            var timer = 5.5;
            var group = this.game.add.group(F84.GameState.Instance.frontLayer);
            this.events.onDestroy.add(function () { return group.destroy(); });
            var pu = group.postUpdate;
            var camera = this.game.camera;
            group.postUpdate = function () {
                pu.call(group);
                group.position.set(camera.x, camera.y);
            };
            var box = this.game.add.sprite(0, 0, 'riddlerBoxClosed', null, group);
            box.anchor.set(0.5);
            box.x = F84.GameState.Instance.bounds.centerX;
            box.y = F84.GameState.Instance.bounds.centerY - 90;
            var boxTween = this.game.add.tween(box);
            boxTween.from({ y: -200 }, 1000, Phaser.Easing.Quadratic.Out, true);
            var shakeTween = this.game.add.tween(box);
            shakeTween.to({ x: box.x + 5 }, 50, Phaser.Easing.Sinusoidal.InOut, true, 800, 10, true);
            return function () {
                var dt = F84.GameState.Instance.deltaTime;
                var prevTimer = timer;
                timer -= dt;
                if (timer <= 0)
                    _this.nextState();
                if (prevTimer >= 3.5 && timer < 3.5) {
                    box.loadTexture('riddlerBoxOpen');
                    var tween = _this.game.add.tween(box);
                    tween.to({ y: box.y - 500 }, 1000, Phaser.Easing.Quadratic.In, true, 2500);
                    tween.onComplete.add(function () { return box.destroy(); });
                    var shockwaveTop_1 = _this.game.add.sprite(box.x, box.y, 'riddlerShockwaveTop', null, group);
                    shockwaveTop_1.anchor.set(0.5);
                    shockwaveTop_1.update = function () { shockwaveTop_1.y -= 480 * dt; };
                    box.events.onDestroy.add(function () { return shockwaveTop_1.destroy(); });
                    var shockwaveBottom_1 = _this.game.add.sprite(box.x, box.y, 'riddlerShockwaveBottom', null, group);
                    shockwaveBottom_1.anchor.set(0.5);
                    shockwaveBottom_1.update = function () { shockwaveBottom_1.y += 480 * dt; };
                    box.events.onDestroy.add(function () { return shockwaveBottom_1.destroy(); });
                    var offset = (_this.game.rnd.frac() * 2 * Math.PI);
                    var _loop_6 = function (i) {
                        var glow = _this.game.add.sprite(0, 0, 'riddlerBulletGlow', null);
                        glow.anchor.set(0.5);
                        F84.GameState.Instance.addObject(glow, group);
                        glow.update = function () {
                            glow.position.set(cube.x, cube.y);
                            glow.angle += 180 * F84.GameState.Instance.deltaTime;
                        };
                        var cube = new F84.Projectile(_this.game, box.x, box.y, 'riddlerCube', 5, 5);
                        F84.GameState.Instance.addObject(cube, group);
                        cube.anchor.set(0.5);
                        cube.frame = i;
                        var angle = (i / 15) * 2 * Math.PI + offset;
                        var speed = (i % 2 == 0) ? 180 : 240;
                        cube.body.velocity.x = Math.cos(angle) * speed;
                        cube.body.velocity.y = Math.sin(angle) * speed;
                        cube.events.onDestroy.add(function () { return glow.pendingDestroy = true; });
                    };
                    for (var i = 0; i < 15; i++) {
                        _loop_6(i);
                    }
                    _this.game.sound.play('riddlerGiftExplode');
                }
            };
        };
        RiddlerBoss.prototype.die = function () {
            var _this = this;
            var timer = 0;
            this.loadTexture('bossBrokenRiddler');
            this.movingY = false;
            this.body.allowGravity = false;
            this.body.velocity.y = 0;
            this.body.enable = false;
            for (var i = 0; i < this.children.length; i++) {
                var child = this.children.pop();
                child.visible = false;
            }
            this.spinOut();
            this.jettisonBoss(this.name);
            var spawnRepeatExplosions = this.repeatExplosionSpawner();
            return function () {
                spawnRepeatExplosions();
                timer += F84.GameState.Instance.deltaTime;
                if (timer > 10)
                    _this.destroy();
            };
        };
        RiddlerBoss.environment = [F84.Environment.Sky, F84.Environment.Water];
        return RiddlerBoss;
    }(F84.Boss));
    F84.RiddlerBoss = RiddlerBoss;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ScrollingBackground = (function (_super) {
        __extends(ScrollingBackground, _super);
        function ScrollingBackground(game, x, y, sprite, bgWidth, xParallax, yParallax) {
            var _this = _super.call(this, game, x, y, sprite) || this;
            _this.baseX = x;
            _this.baseY = y;
            _this.xParallax = xParallax;
            _this.yParallax = (yParallax * 2 + 1) / 3;
            _this.bgWidth = bgWidth;
            _this.updatePosition();
            return _this;
        }
        ScrollingBackground.prototype.postUpdate = function () {
            this.updatePosition();
        };
        ScrollingBackground.prototype.updatePosition = function () {
            var cam = F84.GameState.Instance.camController;
            var cx = cam.bounds.left;
            var cy = cam.bounds.top;
            this.x = this.baseX + cx * this.xParallax;
            this.y = this.baseY + cy * this.yParallax;
            var width = this.width;
            var bounds = cam.bounds;
            if (this.x > bounds.x + bounds.width)
                this.baseX -= this.bgWidth;
            if (this.x + width < bounds.x)
                this.baseX += this.bgWidth;
        };
        return ScrollingBackground;
    }(Phaser.Sprite));
    F84.ScrollingBackground = ScrollingBackground;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Snowball = (function (_super) {
        __extends(Snowball, _super);
        function Snowball(game, x, y, vx, vy) {
            var _this = _super.call(this, game, x, y, 'snowball', 40, 40) || this;
            _this.body.velocity.x = vx;
            _this.body.velocity.y = vy;
            return _this;
        }
        Snowball.prototype.update = function () {
            this.body.velocity.y += 400 * F84.GameState.Instance.deltaTime;
        };
        return Snowball;
    }(F84.Projectile));
    F84.Snowball = Snowball;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Stud = (function (_super) {
        __extends(Stud, _super);
        function Stud(game, x, y) {
            var _this = _super.call(this, game, x, y, 'stud') || this;
            _this.collected = false;
            _this.anchor.set(0.5);
            var frames = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
            if (_this.game.rnd.sign() > 0)
                frames = frames.reverse();
            _this.animations.add('', frames).play(24, true);
            game.physics.enable(_this);
            _this.body.setSize(40, 40, _this.width / 2 - 20, _this.height / 2 - 20);
            return _this;
        }
        Stud.prototype.update = function () {
            if (this.body) {
                var dt = F84.GameState.Instance.deltaTime;
                this.body.velocity.x -= this.body.velocity.x * 3 * dt;
                this.body.velocity.y -= this.body.velocity.y * 3 * dt;
                if (this.magnetized) {
                    var player = F84.GameState.Instance.player;
                    var body = player.body;
                    if (!player.isInputEnabled) {
                        body = player.transformation.body;
                    }
                    var speed = 1000;
                    if (player.boostTimer > 0)
                        speed *= 2;
                    var dx = body.x - this.body.x;
                    var dy = body.y - this.body.y;
                    var angle = Math.atan2(dy, dx);
                    this.body.x += Math.cos(angle) * 400 * dt;
                    this.body.y += Math.sin(angle) * 400 * dt;
                    this.body.velocity.x += Math.cos(angle) * speed * dt;
                    this.body.velocity.y += Math.sin(angle) * speed * dt;
                }
            }
        };
        Stud.prototype.collect = function () {
            this.collected = true;
            this.loadTexture('studCollect');
            this.animations.add('').play(60, false, true);
            this.body.destroy();
        };
        Stud.prototype.magnetize = function () {
            this.magnetized = true;
        };
        return Stud;
    }(Phaser.Sprite));
    F84.Stud = Stud;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var SwayEnemy = (function (_super) {
        __extends(SwayEnemy, _super);
        function SwayEnemy(game, x, y) {
            var _this = this;
            var type = F84.GameState.Instance.upcomingBoss;
            _this = _super.call(this, game, x, y, 'drone') || this;
            _this.timer = 0;
            _this.offset = game.rnd.frac() * Math.PI * 2;
            _this.initialX = x;
            _this.body.setSize(40, 40, _this.width / 2 - 20, _this.height / 2 - 20);
            var addBlades = function (x, y) {
                var blades = game.add.sprite(x, y, 'drone' + type + 'Blades');
                blades.anchor.set(0.5);
                blades.animations.add('').play(16, true);
                _this.addChild(blades);
            };
            addBlades(-14, -11);
            addBlades(14, -11);
            addBlades(-14, 11);
            addBlades(14, 11);
            return _this;
        }
        SwayEnemy.prototype.activate = function () {
            _super.prototype.activate.call(this);
            this.body.velocity.y = -150;
        };
        SwayEnemy.prototype.update = function () {
            _super.prototype.update.call(this);
            if (this.activated) {
                this.timer += F84.GameState.Instance.deltaTime;
                this.x = this.initialX + 100 * Math.sin(this.timer + this.offset);
            }
        };
        return SwayEnemy;
    }(F84.Enemy));
    F84.SwayEnemy = SwayEnemy;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var VehicleTransformation = (function (_super) {
        __extends(VehicleTransformation, _super);
        function VehicleTransformation(game, transformation1, transformation2, finalVehicle) {
            var _this = _super.call(this, game) || this;
            _this.transformation2 = transformation2;
            _this.finalVehicle = finalVehicle;
            _this.position.set(_this.game.camera.x, _this.game.camera.y);
            var bounds = F84.GameState.Instance.bounds;
            var player = F84.GameState.Instance.player;
            player.hide();
            var glow = _this.glow = game.add.sprite(0, 0, 'transformGlow', null, _this);
            glow.anchor.set(0.5);
            glow.alignIn(bounds, Phaser.CENTER);
            glow.update = function () { glow.angle += 180 * F84.GameState.Instance.deltaTime; };
            glow.alpha = 0;
            var glowFadeTween = _this.game.add.tween(glow);
            glowFadeTween.to({ alpha: 1 }, 1000, Phaser.Easing.Linear.None, true);
            var batmobile = _this.batmobile = game.add.sprite(0, 0, transformation1, null, _this);
            batmobile.anchor.set(0.5);
            batmobile.scale.set(0.7);
            batmobile.alignIn(bounds, Phaser.CENTER, 0, -20);
            var hitBox = _this.hitBox = game.add.sprite(0, 0, 'whiteSquare', null, _this);
            hitBox.anchor.set(0.5);
            hitBox.alpha = 0;
            batmobile.addChild(hitBox);
            game.physics.arcade.enable(hitBox);
            _this.body = hitBox.body;
            var scaleInTween = _this.game.tweens.create(batmobile.scale);
            scaleInTween.to({ x: 1, y: 1 }, 300, Phaser.Easing.Cubic.Out, true);
            var playerPos = new Phaser.Point(player.x - _this.x, player.y - _this.y);
            var moveTween = _this.game.add.tween(batmobile);
            moveTween.from({ x: playerPos.x, y: playerPos.y }, 300, Phaser.Easing.Cubic.Out, true);
            player.x = F84.GameState.Instance.bounds.centerX;
            var timer = _this.game.time.create();
            timer.add(200, function () { return _this.deconstruct(batmobile); });
            timer.start();
            _this.game.sound.play('vehicleTransform');
            return _this;
        }
        VehicleTransformation.prototype.deconstruct = function (batmobile) {
            var _this = this;
            batmobile.animations.add('').play(8, false);
            var shakeTween = this.game.add.tween(batmobile);
            shakeTween.to({ x: batmobile.x + 5 }, 50, Phaser.Easing.Sinusoidal.InOut, true, 0, 3, true);
            var _loop_7 = function (i) {
                var key = 'transformComponent';
                if (i < 10)
                    key += '0';
                key += i;
                var piece = this_6.game.add.sprite(batmobile.x + this_6.game.rnd.between(-30, 30), batmobile.y + this_6.game.rnd.between(-30, 30), key, null, this_6);
                piece.anchor.set(0.5);
                piece.scale.set(0.3);
                piece.alpha = 0;
                var angle = Math.atan2(piece.y - batmobile.y, piece.x - batmobile.x);
                var vx = Math.cos(angle) * 900;
                var vy = Math.sin(angle) * 900;
                var delay = (i - 1) * 30;
                var timer_1 = 0;
                piece.update = function () {
                    var dt = F84.GameState.Instance.deltaTime;
                    timer_1 += dt;
                    if (timer_1 >= delay / 1000) {
                        piece.x += vx * dt;
                        piece.y += vy * dt;
                        if (vx < 0)
                            vx += 180 * dt;
                        if (vx > 0)
                            vx -= 180 * dt;
                        if (vy < 0)
                            vy += 180 * dt;
                        if (vy > 0)
                            vy -= 180 * dt;
                    }
                };
                var fadeInTween = this_6.game.add.tween(piece);
                fadeInTween.to({ alpha: 1 }, 25, Phaser.Easing.Linear.None, true, delay);
                var scaleTween = this_6.game.add.tween(piece.scale);
                scaleTween.to({ x: 1, y: 1 }, 500, Phaser.Easing.Linear.None, true, delay);
                var fadeOutTween = this_6.game.add.tween(piece);
                fadeOutTween.to({ alpha: 0 }, 1200, Phaser.Easing.Quadratic.In, false);
                fadeInTween.onComplete.add(function () { return fadeOutTween.start(); });
            };
            var this_6 = this;
            for (var i = 1; i <= 12; i++) {
                _loop_7(i);
            }
            var timer = this.game.time.create();
            timer.add(200, function () { return _this.showBatman(); });
            timer.start();
        };
        VehicleTransformation.prototype.showBatman = function () {
            var _this = this;
            var logo = this.game.add.sprite(this.batmobile.x, this.batmobile.y + 10, 'transformLogo', null, this);
            logo.anchor.set(0.5);
            logo.scale.set(0.1);
            logo.alpha = 0;
            var scaleTween = this.game.add.tween(logo.scale);
            scaleTween.to({ x: 1.2, y: 1.2 }, 800, Phaser.Easing.Cubic.Out, true, 0);
            var fadeTween = this.game.add.tween(logo);
            fadeTween.to({ alpha: 1 }, 400, Phaser.Easing.Linear.None, true, 0);
            var batman2 = this.game.add.sprite(logo.x, logo.y, 'transformBatman2', null, this);
            batman2.anchor.set(0.5);
            batman2.alpha = 0;
            var batman = this.game.add.sprite(logo.x, logo.y, 'transformBatman1', null, this);
            batman.anchor.set(0.6, 0.5);
            batman.scale.set(0.05);
            batman.alpha = 0;
            var batmanScale = this.game.add.tween(batman.scale);
            batmanScale.to({ x: 1, y: 1 }, 600, Phaser.Easing.Back.Out, true, 250);
            var batmanFade = this.game.add.tween(batman);
            batmanFade.to({ alpha: 1 }, 100, Phaser.Easing.Linear.None, true, 250);
            scaleTween.onComplete.add(function () {
                var delay = 300;
                var scaleOutTween = _this.game.add.tween(logo.scale);
                scaleOutTween.to({ x: 0.1, y: 0.1 }, 500, Phaser.Easing.Quadratic.In, true, delay);
                var fadeOutTween = _this.game.add.tween(logo);
                fadeOutTween.to({ alpha: 0 }, 500, Phaser.Easing.Quadratic.In, true, delay);
                var batmanScaleOut = _this.game.add.tween(batman.scale);
                batmanScaleOut.to({ x: 0.1, y: 0.1 }, 300, Phaser.Easing.Quadratic.In, true, delay);
                var batmanFadeOut = _this.game.add.tween(batman);
                batmanFadeOut.to({ alpha: 0 }, 300, Phaser.Easing.Quadratic.In, true, delay);
                batman2.alpha = 1;
                var b2ScaleOut = _this.game.add.tween(batman2.scale);
                b2ScaleOut.to({ x: 0.1, y: 0.1 }, 400, Phaser.Easing.Quadratic.In, true, delay + 200);
                b2ScaleOut.onComplete.add(function () { return batman2.destroy(); });
                var timer = _this.game.time.create();
                timer.add(200, function () { return _this.reconstruct(); });
                timer.start();
            });
        };
        VehicleTransformation.prototype.reconstruct = function () {
            var _this = this;
            var batmobile = this.batmobile;
            batmobile.loadTexture(this.transformation2, 2);
            if (this.transformation2 == 'transformBatwing')
                batmobile.anchor.set(186 / 357, 0.5);
            else
                batmobile.anchor.set(0.5, 0.65);
            batmobile.x = this.glow.x;
            batmobile.y = this.glow.y;
            var _loop_8 = function (i) {
                var key = 'transformComponent';
                if (i < 10)
                    key += '0';
                key += i;
                var piece = this_7.game.add.sprite(batmobile.x + this_7.game.rnd.between(-100, 100), batmobile.y + this_7.game.rnd.between(-100, 100), key, null, this_7);
                piece.anchor.set(0.5);
                piece.scale.set(1);
                piece.alpha = 1;
                var angle = Math.atan2(piece.y - batmobile.y, piece.x - batmobile.x);
                var delay = (i - 1) * 30;
                var moveTween = this_7.game.add.tween(piece);
                moveTween.from({ x: piece.x + Math.cos(angle) * 800, y: piece.y + Math.sin(angle) * 800 }, 800, Phaser.Easing.Cubic.Out, true, delay);
                moveTween.onComplete.add(function () { return piece.destroy(); });
                var fadeOutTween = this_7.game.add.tween(piece);
                fadeOutTween.to({ alpha: 0 }, 100, Phaser.Easing.Linear.None, true, delay + 550);
                var scaleTween = this_7.game.add.tween(piece.scale);
                scaleTween.to({ x: 0.3, y: 0.3 }, 500, Phaser.Easing.Linear.None, true, delay);
            };
            var this_7 = this;
            for (var i = 1; i <= 12; i++) {
                _loop_8(i);
            }
            var shakeTween = this.game.add.tween(batmobile);
            shakeTween.to({ x: batmobile.x + 5 }, 50, Phaser.Easing.Sinusoidal.InOut, true, 400, 3, true);
            var timer = this.game.time.create();
            timer.add(300, function () {
                var anim = batmobile.animations.add('', [2, 1, 0, 0]).play(8);
                anim.onComplete.add(function () {
                    _this.land();
                });
            });
            timer.start();
        };
        VehicleTransformation.prototype.land = function () {
            var _this = this;
            var batmobile = this.batmobile;
            batmobile.loadTexture(this.finalVehicle);
            batmobile.anchor.set(0.5);
            if (this.finalVehicle == 'batmobile')
                batmobile.anchor.set(0.5, 134 / 204);
            batmobile.scale.set(2);
            var batwingScaleTween = this.game.add.tween(batmobile.scale);
            batwingScaleTween.to({ x: 1, y: 1 }, 300, Phaser.Easing.Cubic.Out, true);
            var glowFadeOut = this.game.add.tween(this.glow);
            glowFadeOut.to({ alpha: 0 }, 300, Phaser.Easing.Linear.None, true);
            var player = F84.GameState.Instance.player;
            var playerPos = new Phaser.Point(player.x - this.x, player.y - this.y);
            var moveTween = this.game.add.tween(batmobile);
            moveTween.to({ x: playerPos.x, y: playerPos.y }, 300, Phaser.Easing.Quadratic.Out, true);
            moveTween.onComplete.add(function () {
                player.unhide();
                _this.destroy();
            });
        };
        VehicleTransformation.prototype.postUpdate = function () {
            _super.prototype.postUpdate.call(this);
            this.position.set(this.game.camera.x, this.game.camera.y);
        };
        VehicleTransformation.prototype.onHitObject = function (us, other) {
            if (other instanceof F84.Stud) {
                var player = F84.GameState.Instance.player;
                var value = player.studValue;
                if (value - Math.floor(value) == 0.5) {
                    value = (player.studs % 2 == 0) ? 2 : 1;
                }
                player.studs += value;
                F84.GameState.Instance.score += value;
                other.collect();
                this.game.sound.play('studCollect');
                player.studCombo++;
                if (player.studCombo >= 50) {
                    F84.GameState.Instance.ui.spawnCelebratoryMessage();
                    player.studCombo = 0;
                }
            }
        };
        return VehicleTransformation;
    }(Phaser.Group));
    F84.VehicleTransformation = VehicleTransformation;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var PhaserText = (function () {
        function PhaserText() {
        }
        PhaserText.override = function () {
            Phaser.Text.prototype.setText = function (text, immediate) {
                if (immediate === undefined) {
                    immediate = false;
                }
                var fontTagIndex = text.indexOf('<font');
                if (fontTagIndex != -1) {
                    var endingIndex = text.indexOf('>', fontTagIndex);
                    var fontTag = text.slice(fontTagIndex, endingIndex);
                    var sizeString = fontTag.split('=')[1];
                    sizeString = sizeString.replace(new RegExp('[\'"]', 'g'), '');
                    this.style.fontSize = sizeString;
                    this.fontSize = sizeString;
                    text = text.replace(new RegExp('<[^>]*>'), '');
                    text = text.replace('</font>', '');
                }
                text = text.toString() || '';
                if (text === this._text) {
                    return this;
                }
                this.text = text;
                if (immediate) {
                    this.updateText();
                }
                else {
                    this.dirty = true;
                }
                return this;
            };
        };
        PhaserText._superSetText = Phaser.Text.prototype.setText;
        return PhaserText;
    }());
    F84.PhaserText = PhaserText;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var AnimationList = (function () {
        function AnimationList() {
        }
        AnimationList.add = function (animation) {
            this.animations.push(animation);
        };
        AnimationList.remove = function (animation) {
            F84.ArrayUtils.remove(this.animations, animation);
        };
        AnimationList.animations = [];
        return AnimationList;
    }());
    F84.AnimationList = AnimationList;
    var Animation = (function (_super) {
        __extends(Animation, _super);
        function Animation(game, parent, name, frameData, frames, frameRate, loop) {
            var _this = _super.call(this, game, parent, name, frameData, frames, frameRate, loop) || this;
            AnimationList.add(_this);
            return _this;
        }
        Animation.prototype.destroy = function () {
            AnimationList.remove(this);
            _super.prototype.destroy.call(this);
        };
        return Animation;
    }(Phaser.Animation));
    Phaser.Animation = Animation;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ArrayUtils = (function () {
        function ArrayUtils() {
        }
        ArrayUtils.shuffle = function (array) {
            var currentIndex = array.length;
            var temporaryValue;
            var randomIndex;
            while (0 !== currentIndex) {
                randomIndex = Math.floor(Math.random() * currentIndex);
                currentIndex -= 1;
                temporaryValue = array[currentIndex];
                array[currentIndex] = array[randomIndex];
                array[randomIndex] = temporaryValue;
            }
            return array;
        };
        ArrayUtils.remove = function (array, item) {
            for (var i = 0; i < array.length; i++) {
                if (array[i] == item) {
                    return array.splice(i, 1);
                }
            }
            return array;
        };
        ArrayUtils.clone = function (array) {
            var clone = new Array();
            for (var i = 0; i < array.length; i++) {
                clone.push(array[i]);
            }
            return clone;
        };
        return ArrayUtils;
    }());
    F84.ArrayUtils = ArrayUtils;
})(F84 || (F84 = {}));
function bind(keyName, action, context) {
    var pkc = Phaser.KeyCode;
    var keyCode = pkc[keyName.toUpperCase()];
    var bind = function () {
        var key = F84.Game.Instance.input.keyboard.addKey(keyCode);
        key.onDown.add(action, context);
    };
    bind();
    F84.Game.Instance.state.onStateChange.add(bind);
}
var F84;
(function (F84) {
    var Fonts = (function () {
        function Fonts() {
        }
        Object.defineProperty(Fonts, "DefaultFont", {
            get: function () {
                switch (F84.Game.Instance.locale) {
                    case "cz-cz":
                    case "pl-pl":
                        return "Arial";
                    default:
                        return "20pt roboto-medium";
                }
            },
            enumerable: true,
            configurable: true
        });
        Object.defineProperty(Fonts, "BoldFont", {
            get: function () {
                switch (F84.Game.Instance.locale) {
                    case "cz-cz":
                    case "pl-pl":
                        return "Arial";
                    default:
                        return "20pt roboto-medium";
                }
            },
            enumerable: true,
            configurable: true
        });
        return Fonts;
    }());
    F84.Fonts = Fonts;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Fullscreen = (function () {
        function Fullscreen() {
        }
        Fullscreen.startFullscreen = function (game) {
            if (!game.device.desktop) {
                game.scale.startFullScreen(false);
            }
        };
        return Fullscreen;
    }());
    F84.Fullscreen = Fullscreen;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var HitboxDebug = (function () {
        function HitboxDebug() {
        }
        HitboxDebug.createHitbox = function (game, body) {
            var box = game.add.sprite(body.x, body.y, 'whiteSquare');
            box.width = body.width;
            box.height = body.height;
            box.tint = 0xBBBB44;
            box.alpha = 0.5;
            box.postUpdate = function () { return box.position.set(body.x, body.y); };
            body.sprite.events.onDestroy.add(function () { return box.destroy(); });
        };
        return HitboxDebug;
    }());
    F84.HitboxDebug = HitboxDebug;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var LegoUtils = (function () {
        function LegoUtils() {
        }
        LegoUtils.onLegoSite = function () {
            return F84.GameConfig.get("legoSiteBuild");
        };
        return LegoUtils;
    }());
    F84.LegoUtils = LegoUtils;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Music = (function () {
        function Music() {
        }
        Music.switchMusic = function (game, key) {
            if (key == this.currentKey)
                return;
            if (this.currentMusic)
                this.currentMusic.destroy();
            this.currentMusic = game.sound.play(key, 0.4, true);
            this.currentKey = key;
        };
        Music.stopMusic = function () {
            if (this.currentMusic)
                this.currentMusic.destroy();
            this.currentKey = '';
        };
        return Music;
    }());
    F84.Music = Music;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ObjectUtils = (function () {
        function ObjectUtils() {
        }
        ObjectUtils.isEmpty = function (object) {
            for (var key in object) {
                if (object.hasOwnProperty(key)) {
                    return false;
                }
            }
            return true;
        };
        return ObjectUtils;
    }());
    F84.ObjectUtils = ObjectUtils;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var ScaleBy;
    (function (ScaleBy) {
        ScaleBy[ScaleBy["WIDTH"] = 0] = "WIDTH";
        ScaleBy[ScaleBy["HEIGHT"] = 1] = "HEIGHT";
    })(ScaleBy = F84.ScaleBy || (F84.ScaleBy = {}));
    var OrientationType;
    (function (OrientationType) {
        OrientationType[OrientationType["PORTRAIT"] = 0] = "PORTRAIT";
        OrientationType[OrientationType["LANDSCAPE"] = 1] = "LANDSCAPE";
    })(OrientationType = F84.OrientationType || (F84.OrientationType = {}));
    var Orientation = (function () {
        function Orientation() {
        }
        Orientation.startPortrait = function (state, scaleBy) {
            var _a;
            this.rescale(state, OrientationType.PORTRAIT, scaleBy);
            state.world.rotation = -Math.PI / 2;
            var bounds = state.world.bounds;
            _a = [bounds.height, bounds.width], bounds.width = _a[0], bounds.height = _a[1];
            state.camera.bounds = new Phaser.Rectangle(0, -bounds.width, bounds.height, bounds.width);
        };
        Orientation.endPortrait = function (state) {
            this.rescale(state, OrientationType.LANDSCAPE, ScaleBy.HEIGHT);
            state.world.rotation = 0;
            state.camera.bounds = state.world.bounds;
        };
        Orientation.rescale = function (state, orientation, scaleBy) {
            var desiredWidth = F84.Game.Instance.DefaultWidth;
            var desiredHeight = F84.Game.Instance.DefaultHeight;
            var currentWidth = window.innerWidth;
            var currentHeight = window.innerHeight;
            if (F84.Game.Instance.device.desktop) {
                currentWidth = F84.Game.Instance.DefaultWidth / window.devicePixelRatio;
                currentHeight = F84.Game.Instance.DefaultHeight / window.devicePixelRatio;
            }
            else {
                var minAspectRatio = 700 / 1024;
                if (currentWidth / currentHeight < minAspectRatio) {
                    var mult = minAspectRatio * (currentHeight / currentWidth);
                    desiredHeight *= mult;
                }
            }
            var widthRatio = currentWidth / desiredWidth;
            var heightRatio = currentHeight / desiredHeight;
            if (orientation == OrientationType.LANDSCAPE && scaleBy === ScaleBy.HEIGHT) {
                state.game.scale.setGameSize(currentWidth / heightRatio, desiredHeight);
                state.scale.setUserScale(heightRatio, heightRatio);
            }
            else {
                state.game.scale.setGameSize(desiredWidth, currentHeight / widthRatio);
                state.scale.setUserScale(widthRatio, widthRatio);
            }
            if (state.game.scale.height < state.game.scale.width) {
                if (this._rotateDeviceGroup != null) {
                    this._rotateDeviceGroup.destroy();
                }
                this._rotateDeviceGroup = new F84.RotateDeviceGroup(state.game);
            }
        };
        Orientation.matchBoundsToGameSize = function () {
            var bounds = F84.Game.Instance.getBounds();
            F84.Game.Instance.world.setBounds(bounds.x, bounds.y, bounds.width, bounds.height);
        };
        Orientation.portraitToLandscapePoint = function (game, point) {
            return new Phaser.Point(game.height - point.y, point.x + game.camera.x);
        };
        Orientation._rotateDeviceGroup = null;
        return Orientation;
    }());
    F84.Orientation = Orientation;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Overlay = (function () {
        function Overlay() {
        }
        Overlay.create = function (game, alpha) {
            if (alpha === void 0) { alpha = 0.8; }
            var overlay = game.add.sprite(0, 0, 'whiteSquare');
            overlay.width = game.width;
            overlay.height = game.height;
            overlay.tint = 0x000000;
            overlay.alpha = alpha;
            overlay.inputEnabled = true;
            return overlay;
        };
        return Overlay;
    }());
    F84.Overlay = Overlay;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var SpriteUtils = (function () {
        function SpriteUtils() {
        }
        SpriteUtils.aspectFill = function (sprite, bounds) {
            var x = bounds.width / sprite.width;
            var y = bounds.height / sprite.height;
            var scale = Math.max(x, y);
            sprite.width *= scale;
            sprite.height *= scale;
        };
        SpriteUtils.getAspectFill = function (sprite, bounds) {
            var x = bounds.width / sprite.width;
            var y = bounds.height / sprite.height;
            return Math.max(x, y);
        };
        SpriteUtils.intersects = function (sprite1, sprite2) {
            return !(sprite2.left > sprite1.right
                || sprite2.right < sprite1.left
                || sprite2.top > sprite1.bottom
                || sprite2.bottom < sprite1.top);
        };
        SpriteUtils.queryRect = function (objects, rect) {
            var cols = [];
            for (var _i = 0, objects_2 = objects; _i < objects_2.length; _i++) {
                var obj = objects_2[_i];
                var intersect = !(rect.left > obj.right
                    || rect.right < obj.left
                    || rect.top > obj.bottom
                    || rect.bottom < obj.top);
                if (intersect)
                    cols.push(obj);
            }
            return cols;
        };
        return SpriteUtils;
    }());
    F84.SpriteUtils = SpriteUtils;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var TiledMap = (function () {
        function TiledMap() {
        }
        TiledMap.getProperty = function (obj, property) {
            var p = obj.properties.find(function (p) { return p.name == property; });
            if (p)
                return p.value;
            else
                return null;
        };
        return TiledMap;
    }());
    F84.TiledMap = TiledMap;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var TimeFormat = (function () {
        function TimeFormat() {
        }
        TimeFormat.format = function (seconds) {
            var minutes = Math.floor(seconds / 60);
            var remainder = seconds - (60 * minutes);
            var time = ((minutes > 0) ? minutes : '') + ':' + (remainder < 10 ? '0' : '') + remainder;
            return time;
        };
        return TimeFormat;
    }());
    F84.TimeFormat = TimeFormat;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var TimerList = (function () {
        function TimerList() {
        }
        TimerList.add = function (timer) {
            this.timers.push(timer);
        };
        TimerList.remove = function (timer) {
            F84.ArrayUtils.remove(this.timers, timer);
        };
        TimerList.timers = [];
        return TimerList;
    }());
    F84.TimerList = TimerList;
    var Timer = (function (_super) {
        __extends(Timer, _super);
        function Timer(game, autoDestroy) {
            var _this = _super.call(this, game, autoDestroy) || this;
            TimerList.add(_this);
            return _this;
        }
        Timer.prototype.destroy = function () {
            TimerList.remove(this);
            _super.prototype.destroy.call(this);
        };
        return Timer;
    }(Phaser.Timer));
    Phaser.Timer = Timer;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var Timescale = (function () {
        function Timescale(game, baseFPS, timescale) {
            if (timescale === void 0) { timescale = 1; }
            this.game = game;
            this.baseFPS = baseFPS;
            this.timescale = timescale;
            this.update();
        }
        Timescale.prototype.setTimescale = function (scale) {
            this.timescale = scale;
            this.update();
        };
        Timescale.prototype.setBaseFPS = function (fps) {
            this.baseFPS = fps;
            this.update();
        };
        Timescale.prototype.update = function () {
            this.game.time.slowMotion = 1 / this.timescale;
            this.game.time.desiredFps = this.baseFPS / this.timescale;
        };
        return Timescale;
    }());
    F84.Timescale = Timescale;
})(F84 || (F84 = {}));
var F84;
(function (F84) {
    var UIUtils = (function () {
        function UIUtils() {
        }
        UIUtils.scaleWithCamera = function (obj, camera) {
            var postUpdate = obj.postUpdate;
            var scale = obj.scale.clone();
            var baseScale = new Phaser.Point(1, 1);
            if (obj.data.baseScale)
                baseScale = obj.data.baseScale;
            obj.postUpdate = function () {
                postUpdate.call(obj);
                obj.scale.set((baseScale.x * scale.x) / camera.camScale, (baseScale.y * scale.y) / camera.camScale);
            };
        };
        UIUtils.getMobileBounds = function (bounds) {
            var width = Math.min(bounds.height * (650 / 750), bounds.width);
            var x = bounds.centerX - (width / 2);
            return new Phaser.Rectangle(x, 0, width, F84.Game.Instance.height);
        };
        return UIUtils;
    }());
    F84.UIUtils = UIUtils;
})(F84 || (F84 = {}));
//# sourceMappingURL=game.js.map