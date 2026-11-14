#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

enum class GameState { Title, Playing, Won };

struct Obstacle
{
    sf::RectangleShape shape;
    bool inTopWorld;
    bool deadly;
};

struct Monster
{
    sf::RectangleShape body;   // invisible: used for position and collisions
    float minX;       // leftmost position of the monster's left edge
    float maxX;       // rightmost position of the monster's left edge
    float speed;      // pixels per second
    float direction;  // +1 = moving right, -1 = moving left
};

struct Particle
{
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    float lifetime;   // remaining time in seconds
    float maxLife;    // total time in seconds
};

struct Cloud
{
    sf::Sprite sprite;
    float speed;      // pixels per second, drifting to the right
};

int main()
{
    const float windowWidth = 800.f;
    const float windowHeight = 600.f;
    const float playerSize = 40.f;
    const float monsterSize = 36.f;

    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "SHIFT - Two Realities"
    );
    window.setKeyRepeatEnabled(false);  // one Shift press = one swap

    // Random number generator for particles
    std::mt19937 rng(std::random_device{}());
    auto randomFloat = [&](float min, float max) {
        return std::uniform_real_distribution<float>(min, max)(rng);
    };

    // Particles and visual effects
    std::vector<Particle> particles;

    auto emitParticles = [&](sf::Vector2f pos, sf::Color color, int count, float speed) {
        for (int i = 0; i < count; ++i) {
            Particle p;
            float size = randomFloat(4.f, 8.f);
            p.shape.setSize({size, size});
            p.shape.setPosition(pos);
            p.shape.setFillColor(color);

            float angle = randomFloat(0.f, 2.f * 3.14159f);
            float s = randomFloat(speed * 0.5f, speed * 1.5f);
            p.velocity = {std::cos(angle) * s, std::sin(angle) * s};

            p.maxLife = randomFloat(0.4f, 0.8f);
            p.lifetime = p.maxLife;
            particles.push_back(p);
        }
    };

    // Font (must stay alive as long as any text uses it)
    sf::Font font;
    if (!font.openFromFile("assets/PressStart2P-Regular.ttf"))
    {
        std::cerr << "Could not load assets/PressStart2P-Regular.ttf\n";
        return 1;
    }

    // Textures (must stay alive as long as any sprite uses them)
    sf::Texture playerTexture;
    sf::Texture monsterTexture;
    sf::Texture cloudBigTexture;
    sf::Texture cloudSmallTexture;

    if (!playerTexture.loadFromFile("assets/player.png") ||
        !monsterTexture.loadFromFile("assets/monster.png") ||
        !cloudBigTexture.loadFromFile("assets/cloud_big.png") ||
        !cloudSmallTexture.loadFromFile("assets/cloud_small.png"))
    {
        std::cerr << "Could not load an image from the assets folder "
                     "(player.png, monster.png, cloud_big.png, cloud_small.png)\n";
        return 1;
    }

    // Sprites for the player and the monsters (origin = center, so flips stay in place)
    sf::Sprite playerSprite(playerTexture);
    playerSprite.setOrigin({playerSize / 2.f, playerSize / 2.f});

    sf::Sprite monsterSprite(monsterTexture);
    monsterSprite.setOrigin({monsterSize / 2.f, monsterSize / 2.f});

    float facing = 1.f;   // +1 = looking right, -1 = looking left

    // Sound effects (buffers must stay alive as long as their sounds)
    sf::SoundBuffer jumpBuffer;
    sf::SoundBuffer shiftBuffer;
    sf::SoundBuffer deathBuffer;
    sf::SoundBuffer winBuffer;

    if (!jumpBuffer.loadFromFile("assets/jump.wav"))
        std::cout << "Failed: jump.wav\n";
    else
        std::cout << "Loaded: jump.wav\n";

    if (!shiftBuffer.loadFromFile("assets/shift.wav"))
        std::cout << "Failed: shift.wav\n";
    else
        std::cout << "Loaded: shift.wav\n";

    if (!deathBuffer.loadFromFile("assets/death.wav"))
        std::cout << "Failed: death.wav\n";
    else
        std::cout << "Loaded: death.wav\n";

    if (!winBuffer.loadFromFile("assets/win.wav"))
        std::cout << "Failed: win.wav\n";
    else
        std::cout << "Loaded: win.wav\n";

    sf::Sound jumpSound(jumpBuffer);
    sf::Sound shiftSound(shiftBuffer);
    sf::Sound deathSound(deathBuffer);
    sf::Sound winSound(winBuffer);

    jumpSound.setVolume(30.f);
    shiftSound.setVolume(30.f);
    deathSound.setVolume(30.f);
    winSound.setVolume(40.f);

    // Music: two loops play at the same time and we crossfade between them
    sf::Music calmMusic;
    sf::Music nightmareMusic;

    if (!calmMusic.openFromFile("assets/music_calm.wav"))
        std::cout << "Failed: music_calm.wav\n";
    if (!nightmareMusic.openFromFile("assets/music_nightmare.wav"))
        std::cout << "Failed: music_nightmare.wav\n";

    calmMusic.setLooping(true);
    nightmareMusic.setLooping(true);

    const float musicVolume = 90.f;   // 0 to 100
    float musicMix = 0.f;             // 0 = calm only, 1 = nightmare only

    // Middle platform (centered, so the two worlds mirror each other)
    const float midY = windowHeight / 2.f;
    const float midThickness = 20.f;
    const float platformTop = midY - midThickness / 2.f;     // 290
    const float platformBottom = midY + midThickness / 2.f;  // 310

    sf::RectangleShape middle({windowWidth, midThickness});
    middle.setPosition({0.f, platformTop});
    middle.setFillColor(sf::Color(200, 200, 200));

    // Backgrounds for the two worlds
    sf::RectangleShape topBackground({windowWidth, platformTop});
    topBackground.setPosition({0.f, 0.f});
    topBackground.setFillColor(sf::Color(135, 190, 235));    // bright sky

    sf::RectangleShape bottomBackground({windowWidth, windowHeight - platformBottom});
    bottomBackground.setPosition({0.f, platformBottom});
    bottomBackground.setFillColor(sf::Color(40, 10, 30));    // nightmare

    // Clouds drifting across the sky (top world only)
    std::vector<Cloud> clouds;

    // addCloud(texture, x, y, speed)
    auto addCloud = [&](const sf::Texture& tex, float x, float y, float cloudSpeed)
    {
        Cloud c{sf::Sprite(tex), cloudSpeed};
        c.sprite.setPosition({x, y});
        c.sprite.setColor(sf::Color(255, 255, 255, 235));
        clouds.push_back(c);
    };

    addCloud(cloudBigTexture,    60.f,  70.f, 12.f);
    addCloud(cloudSmallTexture, 330.f,  45.f, 20.f);
    addCloud(cloudBigTexture,   560.f, 130.f,  9.f);
    addCloud(cloudSmallTexture, 700.f,  95.f, 16.f);
    addCloud(cloudSmallTexture, 180.f, 170.f, 14.f);

    // Move the clouds; when one leaves the right edge, bring it back on the left
    auto updateClouds = [&](float dtSeconds)
    {
        for (auto& c : clouds)
        {
            c.sprite.move({c.speed * dtSeconds, 0.f});

            float w = c.sprite.getGlobalBounds().size.x;
            if (c.sprite.getPosition().x > windowWidth)
                c.sprite.setPosition({-w, c.sprite.getPosition().y});
        }
    };

    // Exit door (top world, right edge)
    sf::RectangleShape goal({40.f, 60.f});
    goal.setPosition({740.f, platformTop - 60.f});
    goal.setFillColor(sf::Color(255, 210, 50));
    goal.setOutlineThickness(2.f);
    goal.setOutlineColor(sf::Color(255, 250, 200));

    // Player (this shape is invisible: it only holds the position;
    // the sprite is drawn on top of it)
    const sf::Vector2f startPos{60.f, platformTop - playerSize};
    sf::CircleShape player(playerSize / 2.f);
    player.setFillColor(sf::Color::Green);
    player.setPosition(startPos);

    bool inTopWorld = true;

    // Physics
    sf::Vector2f velocity{0.f, 0.f};
    const float speed = 300.f;
    const float gravity = 1500.f;
    const float jumpSpeed = -650.f;  // away from the platform in the top world
    bool onGround = false;

    // Nightmare meter: 0 = empty, 1 = full (full means you lose)
    float nightmare = 0.f;
    float nightmareFillTime = 7.f;           // seconds to fill (set per level)
    const float nightmareDrainTime = 3.5f;   // seconds to empty in the top world
    float elapsed = 0.f;                     // total time, used for blinking

    // The meter bar (top-left corner)
    const float barWidth = 200.f;
    const float barHeight = 14.f;

    sf::RectangleShape barBack({barWidth, barHeight});
    barBack.setPosition({20.f, 15.f});
    barBack.setFillColor(sf::Color(30, 30, 40));
    barBack.setOutlineThickness(2.f);
    barBack.setOutlineColor(sf::Color::White);

    sf::RectangleShape barFill({0.f, barHeight});
    barFill.setPosition({20.f, 15.f});

    // Game state and level progress
    GameState state = GameState::Title;
    const int totalLevels = 3;
    int currentLevel = 0;

    const std::string levelNames[totalLevels] = {
        "1: Learn to Shift",
        "2: The Nightmare Below",
        "3: Mind the Gap"
    };

    // Texts: in-game
    sf::Text levelText(font, "", 14);
    levelText.setFillColor(sf::Color(20, 30, 50));

    sf::Text hintText(font, "A/D MOVE   SPACE JUMP   SHIFT SWAP WORLDS", 12);
    hintText.setFillColor(sf::Color(200, 200, 220));

    // Texts: title screen
    sf::Text titleText(font, "SHIFT", 48);
    titleText.setFillColor(sf::Color(20, 30, 50));

    sf::Text subtitleText(font, "TWO REALITIES", 20);
    subtitleText.setFillColor(sf::Color(20, 30, 50));

    sf::Text startText(font, "PRESS ENTER TO START", 14);
    startText.setFillColor(sf::Color(230, 230, 240));

    sf::Text jamText(font, "GAME OFF 2026", 10);
    jamText.setFillColor(sf::Color(150, 120, 170));

    // Texts: win screen
    sf::Text winText(font, "YOU ESCAPED!", 36);
    winText.setFillColor(sf::Color(60, 40, 0));

    sf::Text thanksText(font, "THANKS FOR PLAYING", 14);
    thanksText.setFillColor(sf::Color(60, 40, 0));

    sf::Text continueText(font, "PRESS ENTER", 14);
    continueText.setFillColor(sf::Color(60, 40, 0));

    // Helper: center a text horizontally at height y
    auto centerText = [&](sf::Text& t, float y)
    {
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition({(windowWidth - b.size.x) / 2.f - b.position.x, y});
    };

    centerText(hintText, windowHeight - 32.f);
    centerText(titleText, 70.f);
    centerText(subtitleText, 150.f);
    centerText(startText, 400.f);
    centerText(jamText, 560.f);
    centerText(winText, 200.f);
    centerText(thanksText, 290.f);
    centerText(continueText, 400.f);

    // Helper: do the player's square and a rectangle overlap?
    auto overlaps = [](sf::Vector2f p, float size, const sf::RectangleShape& r)
    {
        sf::Vector2f rp = r.getPosition();
        sf::Vector2f rs = r.getSize();
        return p.x < rp.x + rs.x && p.x + size > rp.x &&
               p.y < rp.y + rs.y && p.y + size > rp.y;
    };

    // Helper: send the player back to the start (silent)
    auto restart = [&]()
    {
        player.setPosition(startPos);
        velocity = {0.f, 0.f};
        inTopWorld = true;
        onGround = false;
        nightmare = 0.f;
        facing = 1.f;
    };

    // Helper: the player dies (red burst + sound + restart)
    auto die = [&]()
    {
        sf::Vector2f p = player.getPosition();
        emitParticles(sf::Vector2f(p.x + playerSize / 2.f, p.y + playerSize / 2.f),
                      sf::Color(255, 80, 80), 25, 220.f);

        deathSound.play();
        restart();
    };

    // Helpers: start and stop the music
    auto startMusic = [&]()
    {
        musicMix = 0.f;
        calmMusic.setVolume(musicVolume);
        nightmareMusic.setVolume(0.f);
        calmMusic.play();
        nightmareMusic.play();
    };

    auto stopMusic = [&]()
    {
        calmMusic.stop();
        nightmareMusic.stop();
    };

    // Obstacles
    std::vector<Obstacle> obstacles;

    // addObstacle(inTopWorld, deadly, x, width, height)
    auto addObstacle = [&](bool topWorld, bool deadly, float x, float width, float height)
    {
        Obstacle o{sf::RectangleShape({width, height}), topWorld, deadly};
        float y = topWorld ? platformTop - height : platformBottom;
        o.shape.setPosition({x, y});

        if (deadly)
            o.shape.setFillColor(sf::Color(200, 40, 40));
        else if (topWorld)
            o.shape.setFillColor(sf::Color(120, 90, 60));
        else
            o.shape.setFillColor(sf::Color(110, 60, 120));

        obstacles.push_back(o);
    };

    // Monsters (bottom world only, hanging from the middle platform)
    std::vector<Monster> monsters;

    // addMonster(startX, minX, maxX, speed)
    auto addMonster = [&](float startX, float minX, float maxX, float monsterSpeed)
    {
        Monster m{sf::RectangleShape({monsterSize, monsterSize}),
                  minX, maxX, monsterSpeed, 1.f};
        m.body.setPosition({startX, platformBottom});
        m.body.setFillColor(sf::Color(200, 50, 90));
        monsters.push_back(m);
    };

    // Build a level: clear the old one, add the new obstacles and monsters
    auto loadLevel = [&](int index)
    {
        obstacles.clear();
        monsters.clear();

        if (index == 0)
        {
            // Level 1: learn to shift. The wall is too tall to jump over,
            // and it is solid (not deadly), so exploring is safe.
            nightmareFillTime = 8.f;
            addObstacle(true, false, 380.f, 40.f, 220.f);
        }
        else if (index == 1)
        {
            // Level 2: a wide deadly strip, a hanging block and two monsters
            nightmareFillTime = 7.f;
            addObstacle(true,  false, 120.f,  60.f, 60.f);   // top: solid step
            addObstacle(true,  true,  230.f, 300.f, 30.f);   // top: wide deadly strip
            addObstacle(false, false, 360.f,  60.f, 60.f);   // bottom: hanging block
            addMonster(250.f, 240.f, 320.f, 70.f);
            addMonster(460.f, 440.f, 500.f, 80.f);
        }
        else
        {
            // Level 3: go under the first strip, land in the gap, jump the second
            nightmareFillTime = 8.f;
            addObstacle(true,  true,  180.f, 280.f, 30.f);   // top: wide deadly strip
            addObstacle(true,  true,  540.f, 160.f, 30.f);   // top: shorter strip (jumpable)
            addObstacle(false, false, 300.f,  40.f, 70.f);   // bottom: hanging block
            addMonster(200.f, 190.f, 260.f, 80.f);
            addMonster(370.f, 350.f, 420.f, 100.f);
        }

        restart();
    };

    loadLevel(0);

    // Level progress squares (top-right corner)
    sf::RectangleShape levelSquare({14.f, 14.f});

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();
        elapsed += dt;

        // +1 in the top world (gravity down), -1 in the bottom world (gravity up)
        float dir = inTopWorld ? 1.f : -1.f;

        // Events
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                // Title screen: Enter starts a new game
                if (state == GameState::Title &&
                    keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    state = GameState::Playing;
                    currentLevel = 0;
                    particles.clear();
                    loadLevel(0);
                    startMusic();
                }
                // Win screen: Enter returns to the title
                else if (state == GameState::Won &&
                         keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    state = GameState::Title;
                }
                else if (state == GameState::Playing)
                {
                    // Escape: back to the title screen
                    if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        state = GameState::Title;
                        stopMusic();
                    }

                    // Shift: swap worlds
                    if (keyPressed->code == sf::Keyboard::Key::LShift ||
                        keyPressed->code == sf::Keyboard::Key::RShift)
                    {
                        // Where would the player appear after the shift?
                        sf::Vector2f p = player.getPosition();
                        sf::Vector2f mirrored{p.x, windowHeight - p.y - playerSize};
                        bool targetTop = !inTopWorld;

                        // Blocked if a solid obstacle already occupies that spot
                        bool blocked = false;
                        for (const auto& o : obstacles)
                        {
                            if (o.inTopWorld == targetTop && !o.deadly &&
                                overlaps(mirrored, playerSize, o.shape))
                            {
                                blocked = true;
                            }
                        }

                        if (!blocked)
                        {
                            inTopWorld = targetTop;
                            player.setPosition(mirrored);
                            velocity.y = -velocity.y;
                            dir = inTopWorld ? 1.f : -1.f;
                            shiftSound.play();

                            // Effects
                            emitParticles(sf::Vector2f(player.getPosition().x + playerSize / 2.f,
                                                       player.getPosition().y + playerSize / 2.f),
                                          inTopWorld ? sf::Color::White : sf::Color(150, 50, 200),
                                          15, 150.f);
                        }
                    }
                }
            }
        }

        // ---------- Title screen ----------
        if (state == GameState::Title)
        {
            // Reset the bottom color (the meter tint only applies while playing)
            bottomBackground.setFillColor(sf::Color(40, 10, 30));

            updateClouds(dt);

            window.clear(sf::Color::Black);
            window.draw(topBackground);

            for (const auto& c : clouds)
                window.draw(c.sprite);

            window.draw(bottomBackground);
            window.draw(middle);

            // The player, standing on top and mirrored underneath
            playerSprite.setScale({1.f, 1.f});
            playerSprite.setPosition({windowWidth / 2.f, platformTop - playerSize / 2.f});
            window.draw(playerSprite);

            playerSprite.setScale({1.f, -1.f});
            playerSprite.setPosition({windowWidth / 2.f, platformBottom + playerSize / 2.f});
            window.draw(playerSprite);

            window.draw(titleText);
            window.draw(subtitleText);

            if (static_cast<int>(elapsed * 2.f) % 2 == 0)   // blink
                window.draw(startText);

            window.draw(jamText);
            window.display();
            continue;
        }

        // ---------- Win screen ----------
        if (state == GameState::Won)
        {
            window.clear(sf::Color(255, 210, 50));
            window.draw(winText);
            window.draw(thanksText);

            if (static_cast<int>(elapsed * 2.f) % 2 == 0)   // blink
                window.draw(continueText);

            window.display();
            continue;
        }

        // ---------- Playing ----------

        updateClouds(dt);

        // Update particles: move them, fade them out, remove the dead ones
        for (auto it = particles.begin(); it != particles.end(); )
        {
            it->lifetime -= dt;
            if (it->lifetime <= 0.f)
            {
                it = particles.erase(it);
            }
            else
            {
                it->shape.move(it->velocity * dt);

                sf::Color c = it->shape.getFillColor();
                c.a = static_cast<std::uint8_t>(255.f * (it->lifetime / it->maxLife));
                it->shape.setFillColor(c);
                ++it;
            }
        }

        // Music crossfade: slide musicMix toward 0 (top world) or 1 (bottom world)
        float mixTarget = inTopWorld ? 0.f : 1.f;
        float mixStep = 3.f * dt;   // full fade in about a third of a second

        if (musicMix < mixTarget)
            musicMix = std::min(mixTarget, musicMix + mixStep);
        else if (musicMix > mixTarget)
            musicMix = std::max(mixTarget, musicMix - mixStep);

        calmMusic.setVolume(musicVolume * (1.f - musicMix));
        nightmareMusic.setVolume(musicVolume * musicMix);

        // Horizontal input
        float moveX = 0.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            moveX -= 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            moveX += 1.f;

        // Remember which way the player is facing (for the sprite)
        if (moveX > 0.f)
            facing = 1.f;
        else if (moveX < 0.f)
            facing = -1.f;

        // Jump input (jump direction flips with gravity)
        bool jumpPressed =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

        if (jumpPressed && onGround)
        {
            velocity.y = jumpSpeed * dir;
            onGround = false;
            jumpSound.play();

            // Dust particles at the feet
            sf::Vector2f ppos = player.getPosition();
            float feetY = inTopWorld ? ppos.y + playerSize : ppos.y;
            emitParticles(sf::Vector2f(ppos.x + playerSize / 2.f, feetY),
                          sf::Color(200, 200, 200, 150), 8, 80.f);
        }

        velocity.x = moveX * speed;
        velocity.y += gravity * dir * dt;

        sf::Vector2f pos = player.getPosition();

        // ---- 1) Move horizontally, then push out of solid obstacles ----
        pos.x += velocity.x * dt;

        if (pos.x < 0.f)
            pos.x = 0.f;
        if (pos.x > windowWidth - playerSize)
            pos.x = windowWidth - playerSize;

        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || o.deadly)
                continue;
            if (!overlaps(pos, playerSize, o.shape))
                continue;

            sf::Vector2f op = o.shape.getPosition();
            sf::Vector2f os = o.shape.getSize();

            if (velocity.x > 0.f)
                pos.x = op.x - playerSize;   // hit its left side
            else if (velocity.x < 0.f)
                pos.x = op.x + os.x;         // hit its right side
        }

        // ---- 2) Move vertically, then push out of solid obstacles ----
        pos.y += velocity.y * dt;

        if (pos.y < 0.f)
        {
            pos.y = 0.f;
            velocity.y = 0.f;
        }
        if (pos.y > windowHeight - playerSize)
        {
            pos.y = windowHeight - playerSize;
            velocity.y = 0.f;
        }

        onGround = false;

        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || o.deadly)
                continue;
            if (!overlaps(pos, playerSize, o.shape))
                continue;

            sf::Vector2f op = o.shape.getPosition();
            sf::Vector2f os = o.shape.getSize();

            bool movingTowardGround = (velocity.y * dir > 0.f);

            if (movingTowardGround)
            {
                // Land on the obstacle's surface
                pos.y = inTopWorld ? op.y - playerSize : op.y + os.y;
                onGround = true;
            }
            else
            {
                // Bump into the obstacle's other side
                pos.y = inTopWorld ? op.y + os.y : op.y - playerSize;
            }
            velocity.y = 0.f;
        }

        // Middle platform collision
        if (inTopWorld)
        {
            if (pos.y + playerSize > platformTop)
            {
                pos.y = platformTop - playerSize;
                velocity.y = 0.f;
                onGround = true;
            }
        }
        else
        {
            if (pos.y < platformBottom)
            {
                pos.y = platformBottom;
                velocity.y = 0.f;
                onGround = true;
            }
        }

        player.setPosition(pos);

        // ---- 3) Deadly obstacles (slightly forgiving hitbox) ----
        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || !o.deadly)
                continue;

            if (overlaps({pos.x + 4.f, pos.y + 4.f}, playerSize - 8.f, o.shape))
            {
                die();
                break;
            }
        }

        // ---- 4) Monsters: patrol back and forth ----
        for (auto& m : monsters)
        {
            sf::Vector2f mp = m.body.getPosition();
            mp.x += m.direction * m.speed * dt;

            if (mp.x > m.maxX)
            {
                mp.x = m.maxX;
                m.direction = -1.f;
            }
            if (mp.x < m.minX)
            {
                mp.x = m.minX;
                m.direction = 1.f;
            }

            m.body.setPosition(mp);
        }

        // Monsters only hurt you in the bottom world
        if (!inTopWorld)
        {
            sf::Vector2f p = player.getPosition();

            for (const auto& m : monsters)
            {
                if (overlaps({p.x + 4.f, p.y + 4.f}, playerSize - 8.f, m.body))
                {
                    die();
                    break;
                }
            }
        }

        // ---- 5) Nightmare meter ----
        if (!inTopWorld)
            nightmare += dt / nightmareFillTime;
        else
            nightmare -= dt / nightmareDrainTime;

        if (nightmare < 0.f)
            nightmare = 0.f;

        if (nightmare >= 1.f)
            die();   // the nightmare consumed you

        // ---- 6) Reaching the exit door (top world only) ----
        if (inTopWorld && overlaps(player.getPosition(), playerSize, goal))
        {
            winSound.play();
            currentLevel++;

            if (currentLevel >= totalLevels)
            {
                state = GameState::Won;
                stopMusic();
            }
            else
            {
                loadLevel(currentLevel);
            }
        }

        // Update the bar: width follows the meter, color goes purple -> red
        barFill.setSize({barWidth * nightmare, barHeight});
        barFill.setFillColor(sf::Color(
            static_cast<std::uint8_t>(150.f + 80.f * nightmare),
            static_cast<std::uint8_t>(80.f - 50.f * nightmare),
            static_cast<std::uint8_t>(220.f - 180.f * nightmare)
        ));

        // Warning: blink the outline red when the meter is nearly full
        bool blinkRed = nightmare > 0.75f &&
                        static_cast<int>(elapsed * 6.f) % 2 == 0;
        barBack.setOutlineColor(blinkRed ? sf::Color(255, 60, 60) : sf::Color::White);

        // Dynamic nightmare background (reddens as the meter fills)
        sf::Color bottomColor(
            static_cast<std::uint8_t>(40.f + 60.f * nightmare),
            static_cast<std::uint8_t>(10.f * (1.f - nightmare)),
            static_cast<std::uint8_t>(30.f * (1.f - nightmare))
        );
        bottomBackground.setFillColor(bottomColor);

        // Draw
        window.clear(sf::Color::Black);
        window.draw(topBackground);

        // Clouds go right after the sky so everything else is in front of them
        for (const auto& c : clouds)
            window.draw(c.sprite);

        window.draw(bottomBackground);
        window.draw(middle);
        window.draw(goal);

        for (const auto& o : obstacles)
            window.draw(o.shape);

        // Monsters: upside down (they hang from the platform), gently "breathing"
        for (const auto& m : monsters)
        {
            sf::Vector2f mp = m.body.getPosition();
            float breathe = 1.f + 0.05f * std::sin(elapsed * 6.f + mp.x * 0.05f);

            monsterSprite.setPosition({mp.x + monsterSize / 2.f, mp.y + monsterSize / 2.f});
            monsterSprite.setScale({1.f, -breathe});
            window.draw(monsterSprite);
        }

        // Particles
        for (const auto& p : particles)
            window.draw(p.shape);

        // Player: faces the way it walks, upside down in the bottom world
        sf::Vector2f pp = player.getPosition();
        playerSprite.setPosition({pp.x + playerSize / 2.f, pp.y + playerSize / 2.f});
        playerSprite.setScale({facing, inTopWorld ? 1.f : -1.f});
        window.draw(playerSprite);

        window.draw(barBack);
        window.draw(barFill);

        // Level progress squares
        for (int i = 0; i < totalLevels; i++)
        {
            levelSquare.setPosition({windowWidth - 20.f - (totalLevels - i) * 22.f, 15.f});

            if (i < currentLevel)
                levelSquare.setFillColor(sf::Color(255, 210, 50));    // completed
            else if (i == currentLevel)
                levelSquare.setFillColor(sf::Color::White);           // current
            else
                levelSquare.setFillColor(sf::Color(90, 90, 100));     // still ahead

            window.draw(levelSquare);
        }

        // Level name, centered at the top (only while a real level is active)
        if (currentLevel < totalLevels)
        {
            levelText.setString(levelNames[currentLevel]);
            centerText(levelText, 16.f);
            window.draw(levelText);
        }

        // Controls hint, only on the first level
        if (currentLevel == 0)
            window.draw(hintText);

        window.display();
    }

    return 0;
}