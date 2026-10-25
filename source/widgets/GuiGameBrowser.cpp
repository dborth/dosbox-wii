/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiGameBrowser.cpp
 ***************************************************************************/

#include "libgui/Gui.h"
#include "GuiGameBrowser.h"

#include <algorithm>

// Layout, in design pixels: one rounded card, rows inset from its edge, the scrollbar column down the right
// side. The same as GuiOptionBrowser, with rows tall enough for a tile and two lines of text.
#define CB_PAD				6
#define CB_ROW_HEIGHT		44		// the card is 4 shorter, leaving a gap between rows
#define CB_ROW_GAP			6
#define CB_SB_WIDTH			22
#define CB_SB_INSET			7
#define CB_SB_ARROW			22
#define CB_SB_TRACK_WIDTH	6
#define CB_SB_TRACK_Y		(CB_PAD + CB_SB_ARROW + 6)
#define CB_THUMB_WIDTH		14
#define CB_THUMB_MIN_HEIGHT	24
#define CB_TILE				28
#define CB_TILE_X			8
#define CB_TEXT_X			(CB_TILE_X + CB_TILE + 10)
#define CB_TEXT_PAD			10
#define CB_ACTION_WIDTH		92		// room kept at the right for the action word
#define CB_TITLE_DY			-8		// the title and the line under it sit this far above and below the row's middle
#define CB_DETAIL_DY		10

//! The selected-row tint a little deeper, for the moment the row is clicked
static PixelColor cardRowPressColor(PixelColor tint, PixelColor accent)
{
	PixelColor c = tint;
	c.r = (uint8_t)(tint.r + (accent.r - tint.r) * 0.18f + 0.5f);
	c.g = (uint8_t)(tint.g + (accent.g - tint.g) * 0.18f + 0.5f);
	c.b = (uint8_t)(tint.b + (accent.b - tint.b) * 0.18f + 0.5f);
	return c;
}

//! The small rounded square with a letter in it at the left of a card
class GuiCardTile : public GuiElement
{
	public:
		GuiCardTile()
		{
			width = CB_TILE;
			height = CB_TILE;
			const GuiTheme& theme = GuiTheme::current();
			shape = new GuiShape(CB_TILE, CB_TILE);
			shape->setParent(this);
			shape->setCorners(GuiCorners(8.0f));
			shape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
			letter = new GuiText(nullptr, 20, theme.text);
			letter->setParent(this);
			letter->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		}

		~GuiCardTile()
		{
			delete shape;
			delete letter;
		}

		//!The character on the tile, and whether it is in the accent colour or neutral
		void set(const std::string& c, bool accent)
		{
			const GuiTheme& theme = GuiTheme::current();
			shape->setPaint(GuiPaint().color(accent ? theme.mark : theme.outline));
			letter->setColor(accent ? theme.text : theme.textMuted);
			letter->setText(c.c_str());
		}

		void draw() override
		{
			if(!isVisible())
				return;
			shape->draw();
			letter->draw();
		}

	private:
		GuiShape * shape;
		GuiText * letter;
};

GuiGameBrowser::GuiGameBrowser(int w, int h, GuiCardSource * s)
{
	width = w;
	height = h;
	source = s;
	selectable = true;
	listOffset = 0;
	listChanged = true; // trigger an initial list update
	selectedItem = 0;
	focus = 0; // allow focus
	thumbHeight = 0;
	thumbTravel = 0;
	dragPos = 0;
	dragging = false;
	count = 0;

	const GuiTheme& theme = GuiTheme::current();
	rowWidth = w - CB_PAD - CB_ROW_GAP - CB_SB_WIDTH - CB_SB_INSET;
	trackHeight = h - 2 * CB_SB_TRACK_Y;
	const int rowTop = (h - CARD_PAGESIZE * CB_ROW_HEIGHT) / 2;	// rows centred in the card

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, INPUT_BTN_A);

	// the card everything sits on
	cardShape = new GuiShape(w, h);
	cardShape->setParent(this);
	cardShape->setCorners(GuiCorners(theme.radius));
	cardShape->setStyle(theme.cardStyle());

	// scrollbar: a thin track with a thumb showing the position, and round arrow buttons
	trackShape = new GuiShape(CB_SB_TRACK_WIDTH, trackHeight);
	trackShape->setParent(this);
	trackShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	trackShape->setPosition(-(CB_SB_INSET + (CB_SB_WIDTH - CB_SB_TRACK_WIDTH) / 2), CB_SB_TRACK_Y);
	trackShape->setCapsule();
	trackShape->setPaint(GuiPaint().color(theme.outline));

	thumbShape = new GuiShape(CB_THUMB_WIDTH, trackHeight);
	thumbShape->setCapsule();
	thumbShape->setStyle(GuiShapeStyle(GuiPaint().color(theme.outlineStrong))
		.onFocus(GuiPaint().color(theme.accent)).onPress(GuiPaint().color(theme.accentDark)));
	thumbShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	thumbBtn = new GuiButton(CB_SB_WIDTH, trackHeight);
	thumbBtn->setParent(this);
	thumbBtn->setImage(thumbShape);
	thumbBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	thumbBtn->setPosition(-CB_SB_INSET, CB_SB_TRACK_Y);
	thumbBtn->setSelectable(false);
	thumbBtn->setClickable(false);
	thumbBtn->setHoldable(true);
	thumbBtn->setTrigger(trigHeldA);
	thumbBtn->setVisible(false);

	const GuiPaint controlRest = GuiPaint().color(theme.surfaceRaised).stroke(1.5f, theme.outline);
	const GuiPaint controlLit = GuiPaint().color(theme.accent).stroke(1.5f, theme.accent);
	const GuiPaint controlHeld = GuiPaint().color(theme.accentDark).stroke(1.5f, theme.accentDark);
	const GuiShapeStyle controlStyle = GuiShapeStyle(controlRest).onFocus(controlLit).onPress(controlHeld);

	arrowUpShape = new GuiShape(CB_SB_ARROW, CB_SB_ARROW);
	arrowUpShape->setShape(SHAPE::ELLIPSE);
	arrowUpShape->setStyle(controlStyle);
	arrowUpShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph = new GuiShape(10, 7);
	arrowUpGlyph->setArrow(SHAPE::TRIANGLE_UP, theme.textMuted, theme.textOnAccent);
	arrowUpGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph->setPosition(0, -1);

	arrowDownShape = new GuiShape(CB_SB_ARROW, CB_SB_ARROW);
	arrowDownShape->setShape(SHAPE::ELLIPSE);
	arrowDownShape->setStyle(controlStyle);
	arrowDownShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph = new GuiShape(10, 7);
	arrowDownGlyph->setArrow(SHAPE::TRIANGLE_DOWN, theme.textMuted, theme.textOnAccent);
	arrowDownGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph->setPosition(0, 1);

	arrowUpBtn = new GuiButton(CB_SB_WIDTH, CB_SB_ARROW);
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpShape);
	arrowUpBtn->setIcon(arrowUpGlyph);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setPosition(-CB_SB_INSET, CB_PAD);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setTrigger(trigA);

	arrowDownBtn = new GuiButton(CB_SB_WIDTH, CB_SB_ARROW);
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownShape);
	arrowDownBtn->setIcon(arrowDownGlyph);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setPosition(-CB_SB_INSET, -CB_PAD);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setTrigger(trigA);

	// A card has an outline at rest; one for something not added yet is fainter and has none of the card's fill; text between
	// cards has neither. All three take the same tint when selected, and a deeper one while clicked.
	const GuiPaint rowLit = GuiPaint().color(theme.highlight).stroke(1.5f, theme.accent);
	const GuiPaint rowHeld = GuiPaint().color(cardRowPressColor(theme.highlight, theme.accent)).stroke(1.5f, theme.accent);
	cardStyle = GuiShapeStyle(GuiPaint().color(theme.surfaceRaised).stroke(1.5f, theme.outline)).onFocus(rowLit).onPress(rowHeld);
	dashedStyle = GuiShapeStyle(GuiPaint().stroke(1.5f, theme.outline)).onFocus(rowLit).onPress(rowHeld);
	plainStyle = GuiShapeStyle(GuiPaint()).onFocus(rowLit).onPress(rowHeld);

	const int textW = std::max(rowWidth - CB_TEXT_X - CB_ACTION_WIDTH - CB_TEXT_PAD, 20);

	for(int i=0; i<CARD_PAGESIZE; i++)
	{
		titleTxt[i] = new GuiText(nullptr, 18, theme.text);
		titleTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		titleTxt[i]->setPosition(CB_TEXT_X, CB_TITLE_DY);
		titleTxt[i]->setMaxWidth(textW);

		detailTxt[i] = new GuiText(nullptr, 12, theme.textMuted);
		detailTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		detailTxt[i]->setPosition(CB_TEXT_X, CB_DETAIL_DY);
		detailTxt[i]->setMaxWidth(textW);

		actionTxt[i] = new GuiText(nullptr, 14, theme.accentDark);
		actionTxt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
		actionTxt[i]->setPosition(-CB_TEXT_PAD, 0);
		actionTxt[i]->setMaxWidth(CB_ACTION_WIDTH);

		tile[i] = new GuiCardTile();
		tile[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		tile[i]->setPosition(CB_TILE_X, 0);

		optionBg[i] = new GuiShape(rowWidth, CB_ROW_HEIGHT - 4);
		optionBg[i]->setCorners(GuiCorners(10.0f));
		optionBg[i]->setStyle(cardStyle);
		optionBg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

		optionBtn[i] = new GuiButton(rowWidth, CB_ROW_HEIGHT);
		optionBtn[i]->setParent(this);
		optionBtn[i]->setLabel(titleTxt[i], 0);
		optionBtn[i]->setLabel(detailTxt[i], 1);
		optionBtn[i]->setLabel(actionTxt[i], 2);
		optionBtn[i]->setImage(optionBg[i]);
		optionBtn[i]->setIcon(tile[i]);
		optionBtn[i]->setPosition(CB_PAD, rowTop + CB_ROW_HEIGHT * i);
		optionBtn[i]->setTrigger(trigA);
		optionIndex[i] = -1;
		rowKind[i] = GuiCardInfo::CARD;
	}
}

GuiGameBrowser::~GuiGameBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete thumbBtn;

	delete cardShape;
	delete trackShape;
	delete thumbShape;
	delete arrowUpShape;
	delete arrowDownShape;
	delete arrowUpGlyph;
	delete arrowDownGlyph;

	delete trigA;
	delete trigHeldA;

	for(int i=0; i<CARD_PAGESIZE; i++)
	{
		delete titleTxt[i];
		delete detailTxt[i];
		delete actionTxt[i];
		delete tile[i];
		delete optionBg[i];
		delete optionBtn[i];
	}
}

//! Sizes and places the scrollbar thumb for the rows currently shown
void GuiGameBrowser::updateThumb()
{
	if(count <= CARD_PAGESIZE)
	{
		thumbBtn->setVisible(false);
		thumbTravel = 0;
		return;
	}

	thumbHeight = std::max(CB_THUMB_MIN_HEIGHT, trackHeight * CARD_PAGESIZE / count);
	thumbTravel = trackHeight - thumbHeight;

	// while dragged the thumb follows the pointer exactly; otherwise it sits at the list position
	int y = dragging ? dragPos : thumbTravel * listOffset / (count - CARD_PAGESIZE);
	if(y > thumbTravel) y = thumbTravel;

	thumbBtn->setVisible(true);
	thumbBtn->setSize(CB_SB_WIDTH, thumbHeight);
	thumbShape->setSize(CB_THUMB_WIDTH, thumbHeight);
	thumbBtn->setMinY(0);
	thumbBtn->setMaxY(thumbTravel);
	thumbBtn->setPosition(-CB_SB_INSET, CB_SB_TRACK_Y + y);
}

//! Puts row index of the source on a slot of the page: its text, its look
void GuiGameBrowser::showRow(int slot, int index)
{
	const GuiTheme& theme = GuiTheme::current();
	GuiCardInfo info;
	source->card(index, info);

	optionIndex[slot] = index;
	rowKind[slot] = info.kind;

	const bool card = info.kind == GuiCardInfo::CARD || info.kind == GuiCardInfo::DASHED;
	optionBg[slot]->setStyle(info.kind == GuiCardInfo::CARD ? cardStyle : info.kind == GuiCardInfo::DASHED ? dashedStyle : plainStyle);

	titleTxt[slot]->setText(info.title.c_str());
	titleTxt[slot]->setColor(card ? theme.text : theme.textMuted);
	detailTxt[slot]->setText(info.detail.c_str());
	actionTxt[slot]->setText(info.action.c_str());
	actionTxt[slot]->setColor(info.accent ? theme.accentDark : theme.textMuted);

	tile[slot]->setVisible(card && !info.tile.empty());
	tile[slot]->set(info.tile, info.accent);

	// text with no tile starts where the tile would
	const int x = tile[slot]->isVisible() ? CB_TEXT_X : CB_TILE_X + 4;
	titleTxt[slot]->setPosition(x, info.detail.empty() ? 0 : CB_TITLE_DY);
	detailTxt[slot]->setPosition(x, CB_DETAIL_DY);
}

void GuiGameBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<CARD_PAGESIZE; i++)
		optionBtn[i]->resetState();

	if(f == 1)
		optionBtn[selectedItem]->setState(STATE::SELECTED);
}

void GuiGameBrowser::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}

	for(int i=0; i<CARD_PAGESIZE; i++)
		optionBtn[i]->resetState();
}

int GuiGameBrowser::getClickedOption()
{
	int found = -1;
	for(int i=0; i<CARD_PAGESIZE; i++)
	{
		if(optionBtn[i]->getState() == STATE::CLICKED)
		{
			optionBtn[i]->setState(STATE::SELECTED);
			found = optionIndex[i];
			break;
		}
	}
	return found;
}

int GuiGameBrowser::getSelectedOption() const
{
	return optionIndex[selectedItem];
}

void GuiGameBrowser::draw()
{
	if(!this->isVisible())
		return;

	cardShape->draw();

	for(int i=0; i<CARD_PAGESIZE; ++i)
	{
		if(optionBtn[i]->isVisible())
			optionBtn[i]->draw();
	}

	trackShape->draw();
	thumbBtn->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();

	this->updateEffects();
}

void GuiGameBrowser::triggerUpdate()
{
	listChanged = true;
}

void GuiGameBrowser::resetText()
{
	for(int i=0; i<CARD_PAGESIZE; i++)
		optionBtn[i]->resetText();
}

void GuiGameBrowser::readRows()
{
	if(!listChanged)
		return;

	{
		listChanged = false;
		count = source->cardCount();

		// the list may have got shorter
		if(listOffset > count - CARD_PAGESIZE)
			listOffset = count - CARD_PAGESIZE;
		if(listOffset < 0)
			listOffset = 0;

		updateThumb();

		for(int i=0; i<CARD_PAGESIZE; ++i)
		{
			const int index = listOffset + i;
			if(index < count)
			{
				if(optionBtn[i]->getState() == STATE::DISABLED)
				{
					optionBtn[i]->setVisible(true);
					optionBtn[i]->setState(STATE::DEFAULT);
				}
				showRow(i, index);
			}
			else
			{
				optionBtn[i]->setVisible(false);
				optionBtn[i]->setState(STATE::DISABLED);
				optionIndex[i] = -1;
			}
		}

		// the highlight must be on a row that is there
		while(selectedItem > 0 && !optionBtn[selectedItem]->isVisible())
			selectedItem--;
	}
}

void GuiGameBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();

	// drag the scrollbar thumb: the list follows the pointer
	if(thumbBtn->isVisible())
		thumbBtn->update(controller);

	const bool wasDragging = dragging;
	dragging = thumbBtn->isVisible() && thumbTravel > 0 &&
		thumbBtn->getState() == STATE::HELD &&
		thumbBtn->getStateChan() == currentChan &&
		pad.validPointer;

	if(dragging)
	{
		// put the thumb's center under the pointer
		int pos = pad.cursor_y - (this->getTop() + CB_SB_TRACK_Y + thumbHeight / 2);
		if(pos < 0) pos = 0;
		else if(pos > thumbTravel) pos = thumbTravel;
		dragPos = pos;

		listOffset = (pos * (count - CARD_PAGESIZE) + thumbTravel / 2) / thumbTravel;
		listChanged = true;
	}
	else if(wasDragging)
		listChanged = true; // snap the thumb to the list position

	readRows();

	// A InputController with no live signal this frame must not be able to claim or evict a selection just by taking
	// its turn through this loop: only a channel that is pointing or pressing gets a say (as in GuiOptionBrowser).
	bool channelActive = pad.validPointer || pad.buttons_d != 0 || pad.buttons_h != 0;

	for(int i=0; i<CARD_PAGESIZE; ++i)
	{
		if(i != selectedItem && optionBtn[i]->getState() == STATE::SELECTED)
			optionBtn[i]->resetState();
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::DEFAULT)
			optionBtn[selectedItem]->setState(STATE::SELECTED, currentChan);
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::SELECTED &&
			optionBtn[i]->getStateChan() != -1 && optionBtn[i]->getStateChan() != currentChan)
			// a reused slot carrying a stale channel: without this a click from the real channel is ignored
			optionBtn[i]->resetState();

		// present "no channel" to any row the cursor is not over, so a stale channel cannot block clicks
		if(pad.validPointer && !optionBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
			controller->setChannel(-1);

		optionBtn[i]->update(controller);
		controller->setChannel(currentChan);

		if(optionBtn[i]->getState() == STATE::SELECTED)
			selectedItem = i;

		// long text scrolls while its row is selected, rather than running into its neighbour
		const SCROLL scroll = selectedItem == i ? SCROLL::HORIZONTAL : SCROLL::NONE;
		titleTxt[i]->setScroll(scroll);
		detailTxt[i]->setScroll(scroll);
	}

	// pad/joystick navigation
	if(!focus)
		return; // skip navigation

	if(controller->down() || arrowDownBtn->getState() == STATE::CLICKED)
	{
		if(optionIndex[selectedItem] >= 0 && optionIndex[selectedItem] + 1 < count)
		{
			if(selectedItem == CARD_PAGESIZE-1)
			{
				// move list down by 1
				listOffset++;
				listChanged = true;
			}
			else if(optionBtn[selectedItem+1]->isVisible())
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem+1]->setState(STATE::SELECTED, controller->getChannel());
				++selectedItem;
			}
		}
		arrowDownBtn->resetState();
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		if(optionIndex[selectedItem] > 0)
		{
			if(selectedItem == 0)
			{
				// move list up by 1
				listOffset--;
				listChanged = true;
			}
			else
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem-1]->setState(STATE::SELECTED, controller->getChannel());
				--selectedItem;
			}
		}
		arrowUpBtn->resetState();
	}

	if(updateCB)
		updateCB(this);
}
