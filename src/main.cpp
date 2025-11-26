#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/utils/VMTHookManager.hpp>

#include <camila/utils.hpp>

using namespace geode::prelude;

CCTexture2D* g_texture = nullptr;

$execute {
	auto ob = new cppreactive::Observatory;

	g_texture = CCSprite::create("sliderBar.png")->getTexture();
	g_texture->retain();

	ob->reactToChanges([&] {
		auto path = *camila::Setting<"wave-image", std::filesystem::path>();

		if (auto spr = CCSprite::create(string::pathToString(path).c_str())) {
			if (g_texture)
				g_texture->release();
			g_texture = spr->getTexture();
			g_texture->retain();
		}
	});
}

class StreakClip : public CCClippingNode {
	void visit() override {
		m_pStencil->setVisible(true);

		CCClippingNode::visit();

		m_pStencil->setVisible(false);
	}
};

class $modify(PlayerObject) {
	struct Fields {
		StreakClip* m_newStreak = nullptr;

		float m_cachedAngle = 0;
		float m_cachedTime = 0;
		CCPoint m_cachedPos = CCPointZero;
	};

	bool init(int player, int ship, GJBaseGameLayer* gameLayer, cocos2d::CCLayer* layer, bool playLayer) {
		if (!PlayerObject::init(player, ship, gameLayer, layer, playLayer))
			return false;

		m_fields->m_newStreak = new StreakClip;
		m_fields->m_newStreak->init(m_waveTrail);
		m_fields->m_newStreak->autorelease();

		if (m_waveTrail)
			m_waveTrail->getParent()->addChild(m_fields->m_newStreak);
		newStreak(CCPointZero, 0);

		return true;
	}

	CCSprite* newStreak(CCPoint const& pos, float rotation) {
		auto spr = CCSprite::createWithTexture(g_texture);

		spr->setAnchorPoint({0, 0.5});
		spr->setRotation(rotation);
		spr->setPosition(pos);
		spr->setScale(camila::Setting<"trail-size", float>()->get() / spr->getContentHeight());

		m_waveTrail->m_waveSize = spr->getScaledContentHeight() / 10.;

		m_fields->m_newStreak->addChild(spr);

		m_fields->m_cachedPos = getPosition();
		return spr;
	}

	void update(float dt) {
		PlayerObject::update(dt);

		if (auto pl = PlayLayer::get()) {
			if (m_isDart) {
				updateNewStreak();			
			}

			m_fields->m_cachedTime = pl->m_gameState.m_levelTime;
			m_fields->m_cachedPos = getPosition();
		}
	}

	void updateNewStreak() {
		if (*camila::Setting<"no-pulse">())
			m_waveTrail->m_pulseSize = 1.5;

		auto diff = getPosition() - m_fields->m_cachedPos;
		diff.x = fabs(diff.x);
		auto angle = (diff).getAngle();

		bool changeAngle = fabs(angle - m_fields->m_cachedAngle) > 0.001 && m_fields->m_cachedPos != getPosition();		

		if (auto lastItem = static_cast<CCNode*>(m_fields->m_newStreak->getChildren()->lastObject())) {
			if (changeAngle || (ccpDistance(lastItem->getPosition(), getPosition() /*- ccp(0, 105)*/) >= lastItem->getScaledContentSize().width)) {
				auto lastPos = lastItem->getPosition();
				auto width = lastItem->getScaledContentSize().width;
				auto angle = m_fields->m_cachedAngle;

				CCPoint newPos = lastPos + CCPoint(fabs(cosf(angle)), sinf(angle)) * width;
				newStreak(newPos, lastItem->getRotation());

				log::info("new streak at {}. player pos: {}", newPos, getPosition());
			}
		}

		if (changeAngle) {
			m_fields->m_cachedAngle = angle;
			log::info("new angle");
			newStreak(getPosition(), -angle * (180. / 3.14159));
		}

		if (PlayLayer::get()->m_gameState.m_levelTime < m_fields->m_cachedTime) {
			m_fields->m_newStreak->removeAllChildren();
		}

		if (auto children = m_fields->m_newStreak->getChildrenExt<CCSprite>(); children.size() > 2) {
			for (auto child : {children[0], children.begin()[1]}) {
				float worldX = m_fields->m_newStreak->convertToWorldSpace(child->getPosition()).x;

				if (worldX < -child->getContentWidth())
					child->removeFromParent();
			}
		}
	}
};
