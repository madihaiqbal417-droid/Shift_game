#include <SFML/Graphics.hpp>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

struct Obstacle
{
    sf::RectangleShape shape;
    bool inTopWorld;
    bool deadly;
};

struct Monster
{
    sf::RectangleShape body;
    float minX;       // leftmost position of the monster's left edge
    float maxX;       // rightmost position of the monster's left edge
    float speed;      // pixels per second
    float direction;  // +1 = moving right, -1 = moving left
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

    // Font (must stay alive as long as any text uses it)
    sf::Font font;
    if (!font.openFromFile("assets/PressStart2P-Regular.ttf"))
    {
        std::cerr << "Could not load assets/PressStart2P-Regular.ttf\n";
        return 1;
    }

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

    // Exit door (top world, right edge)
    sf::RectangleShape goal({40.f, 60.f});
    goal.setPosition({740.f, platformTop - 60.f});
    goal.setFillColor(sf::Color(255, 210, 50));
    goal.setOutlineThickness(2.f);
    goal.setOutlineColor(sf::Color(255, 250, 200));

    // Player
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

    // Level progress
    const int totalLevels = 3;
    int currentLevel = 0;
    bool gameWon = false;

    const std::string levelNames[totalLevels] = {
        "1: Learn to Shift",
        "2: The Nightmare Below",
        "3: Mind the Gap"
    };

    // Text: level name (top, centered) and controls hint (bottom, centered)
    sf::Text levelText(font, "", 14);
    levelText.setFillColor(sf::Color(20, 30, 50));

    sf::Text hintText(font, "A/D MOVE   SPACE JUMP   SHIFT SWAP WORLDS", 12);
    hintText.setFillColor(sf::Color(200, 200, 220));
    {
        sf::FloatRect hb = hintText.getLocalBounds();
        hintText.setPosition({(windowWidth - hb.size.x) / 2.f - hb.position.x,
                              windowHeight - 32.f});
    }

    // Helper: do the player's square and a rectangle overlap?
    auto overlaps = [](sf::Vector2f p, float size, const sf::RectangleShape& r)
    {
        sf::Vector2f rp = r.getPosition();
        sf::Vector2f rs = r.getSize();
        return p.x < rp.x + rs.x && p.x + size > rp.x &&
               p.y < rp.y + rs.y && p.y + size > rp.y;
    };

    // Helper: send the player back to the start
    auto restart = [&]()
    {
        player.setPosition(startPos);
        velocity = {0.f, 0.f};
        inTopWorld = true;
        onGround = false;
        nightmare = 0.f;
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

    // Monster eyes (one shape reused for all monsters)
    sf::CircleShape eye(4.f);
    eye.setFillColor(sf::Color(255, 230, 80));

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
                // Enter: play again after winning
                if (gameWon && keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    gameWon = false;
                    currentLevel = 0;
                    loadLevel(0);
                }

                if (!gameWon &&
                    (keyPressed->code == sf::Keyboard::Key::LShift ||
                     keyPressed->code == sf::Keyboard::Key::RShift))
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
                    }
                }
            }
        }

        // Win screen: nothing else updates until Enter is pressed
        if (gameWon)
        {
            window.clear(sf::Color(255, 210, 50));
            window.display();
            continue;
        }

        // Horizontal input
        float moveX = 0.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            moveX -= 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            moveX += 1.f;

        // Jump input (jump direction flips with gravity)
        bool jumpPressed =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

        if (jumpPressed && onGround)
        {
            velocity.y = jumpSpeed * dir;
            onGround = false;
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
                restart();
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
                    restart();
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
            restart();   // the nightmare consumed you

        // ---- 6) Reaching the exit door (top world only) ----
        if (inTopWorld && overlaps(player.getPosition(), playerSize, goal))
        {
            currentLevel++;

            if (currentLevel >= totalLevels)
                gameWon = true;
            else
                loadLevel(currentLevel);
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

        // Draw
        window.clear(sf::Color::Black);
        window.draw(topBackground);
        window.draw(bottomBackground);
        window.draw(middle);
        window.draw(goal);

        for (const auto& o : obstacles)
            window.draw(o.shape);

        for (const auto& m : monsters)
        {
            window.draw(m.body);

            sf::Vector2f mp = m.body.getPosition();
            eye.setPosition({mp.x + 8.f, mp.y + 18.f});
            window.draw(eye);
            eye.setPosition({mp.x + 24.f, mp.y + 18.f});
            window.draw(eye);
        }

        window.draw(player);

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
            sf::FloatRect lb = levelText.getLocalBounds();
            levelText.setPosition({(windowWidth - lb.size.x) / 2.f - lb.position.x, 16.f});
            window.draw(levelText);
        }

        // Controls hint, only on the first level
        if (currentLevel == 0)
            window.draw(hintText);

        window.display();
    }

    return 0;
}