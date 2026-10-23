#include "QtRocket/gui/figure/Ruler.h"

#include <array>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <QImage>
#include <QPoint>
#include <QScrollBar>
#include <QSize>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Geometry2D.h"
#include "TestScaleFigure.h"

namespace QtRocket::Gui
{

namespace
{

using RulerTest = Test::GuiTest;
using Test::TestScaleFigure;

/// A subject 0.4 m long and 0.1 m across.
constexpr Rectangle2D kSubject{.x = 0.0, .y = -0.05, .width = 0.4, .height = 0.1};

[[nodiscard]] const Unit& lengthUnit(const char* name)
{
    const Unit* unit = unitGroup(UnitGroupId::LENGTH).getUnit(name);
    EXPECT_NE(unit, nullptr) << name;
    return unit == nullptr ? unitGroup(UnitGroupId::LENGTH).getDefaultUnit() : *unit;
}

/// The marks as ScaleProbe (tools/openrocket-gui-pins/scale-figure/probe/ScaleProbe.java) writes
/// them: "n=<count>", then " <position>:<length>" for each tick, with ":<label>" for a major one
/// and ":b" when its label is bold.
[[nodiscard]] std::string describe(const std::vector<Ruler::Mark>& marks)
{
    std::string text = "n=" + std::to_string(marks.size());
    for (const Ruler::Mark& mark : marks)
    {
        text += " " + std::to_string(mark.position) + ":" + std::to_string(mark.length);
        if (!mark.label.isEmpty())
        {
            text += ":" + QtStrings::toUtf8(mark.label) + (mark.bold ? ":b" : "");
        }
    }
    return text;
}

/// OpenRocket's ruler for a scale, a unit, the visible part of the figure (x, y, width, height)
/// and the figure's origin: ScaleProbe.rulers(), Ruler.paintComponent's and drawTick's
/// arithmetic copied into a probe and run against OpenRocket 5f164fd0e's Unit.getTicks and
/// Unit.toString.
struct RulerCase
{
    double             scale{};
    const char*        unit{};
    std::array<int, 4> view{};
    QPoint             origin;
    Qt::Orientation    orientation{Qt::Horizontal};
    const char*        expected{};
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array kRulerCases{
    RulerCase{3779.5296, "m", {0, 0, 600, 400}, {30, 200}, Qt::Horizontal, "n=158 5:3 8:3 12:6 16:3 20:3 23:3 27:3 30:10:0:b 34:3 38:3 41:3 45:3 49:6 53:3 56:3 60:3 64:3 68:10:0.01 72:3 75:3 79:3 83:3 87:6 90:3 94:3 98:3 102:3 106:10:0.02 109:3 113:3 117:3 121:3 124:6 128:3 132:3 136:3 140:3 143:10:0.03 147:3 151:3 155:3 159:3 162:6 166:3 170:3 174:3 177:3 181:10:0.04 185:3 189:3 193:3 196:3 200:6 204:3 208:3 211:3 215:3 219:10:0.05 223:3 227:3 230:3 234:3 238:6 242:3 245:3 249:3 253:3 257:10:0.06 261:3 264:3 268:3 272:3 276:6 279:3 283:3 287:3 291:3 295:10:0.07 298:3 302:3 306:3 310:3 313:6 317:3 321:3 325:3 329:3 332:10:0.08 336:3 340:3 344:3 347:3 351:6 355:3 359:3 363:3 366:3 370:10:0.09 374:3 378:3 381:3 385:3 389:6 393:3 397:3 400:3 404:3 408:10:0.1:b 412:3 416:3 419:3 423:3 427:6 431:3 434:3 438:3 442:3 446:10:0.11 450:3 453:3 457:3 461:3 465:6 468:3 472:3 476:3 480:3 484:10:0.12 487:3 491:3 495:3 499:3 502:6 506:3 510:3 514:3 518:3 521:10:0.13 525:3 529:3 533:3 536:3 540:6 544:3 548:3 552:3 555:3 559:10:0.14 563:3 567:3 570:3 574:3 578:6 582:3 586:3 589:3 593:3 597:10:0.15"},
    RulerCase{3779.5296, "m", {0, 0, 600, 400}, {30, 200}, Qt::Vertical, "n=105 396:3 392:3 388:10:-0.05 384:3 380:3 377:3 373:3 369:6 365:3 362:3 358:3 354:3 350:10:-0.04 346:3 343:3 339:3 335:3 331:6 328:3 324:3 320:3 316:3 312:10:-0.03 309:3 305:3 301:3 297:3 293:6 290:3 286:3 282:3 278:3 275:10:-0.02 271:3 267:3 263:3 259:3 256:6 252:3 248:3 244:3 241:3 237:10:-0.01 233:3 229:3 225:3 222:3 218:6 214:3 210:3 207:3 203:3 200:10:0:b 196:3 192:3 189:3 185:3 181:6 177:3 174:3 170:3 166:3 162:10:0.01 158:3 155:3 151:3 147:3 143:6 140:3 136:3 132:3 128:3 124:10:0.02 121:3 117:3 113:3 109:3 106:6 102:3 98:3 94:3 90:3 87:10:0.03 83:3 79:3 75:3 71:3 68:6 64:3 60:3 56:3 53:3 49:10:0.04 45:3 41:3 37:3 34:3 30:6 26:3 22:3 19:3 15:3 11:10:0.05 7:3 3:3"},
    RulerCase{3779.5296, "cm", {-30, 0, 300, 200}, {437, 301}, Qt::Horizontal, "n=79 -27:3 -23:3 -19:3 -16:10:-12 -12:3 -8:3 -4:3 0:3 3:6 7:3 11:3 15:3 18:3 22:10:-11 26:3 30:3 34:3 37:3 41:6 45:3 49:3 52:3 56:3 60:10:-10:b 64:3 68:3 71:3 75:3 79:6 83:3 87:3 90:3 94:3 98:10:-9 102:3 105:3 109:3 113:3 117:6 121:3 124:3 128:3 132:3 136:10:-8 139:3 143:3 147:3 151:3 155:6 158:3 162:3 166:3 170:3 173:10:-7 177:3 181:3 185:3 189:3 192:6 196:3 200:3 204:3 207:3 211:10:-6 215:3 219:3 223:3 226:3 230:6 234:3 238:3 241:3 245:3 249:10:-5 253:3 257:3 260:3 264:3 268:6"},
    RulerCase{1000.0, "m", {-30, 0, 300, 200}, {437, 301}, Qt::Horizontal, "n=60 -27:3 -22:6 -17:3 -12:10:-0.45 -7:3 -2:6 3:3 8:6 13:3 18:6 23:3 28:6 33:3 38:10:-0.4:b 43:3 48:6 53:3 58:6 63:3 68:6 73:3 78:6 83:3 88:10:-0.35 93:3 98:6 103:3 108:6 113:3 118:6 123:3 128:6 133:3 138:10:-0.3:b 143:3 148:6 153:3 158:6 163:3 168:6 173:3 178:6 183:3 188:10:-0.25 193:3 198:6 203:3 208:6 213:3 218:6 223:3 228:6 233:3 238:10:-0.2:b 243:3 248:6 253:3 258:6 263:3 268:6"},
    RulerCase{1000.0, "ft", {0, 0, 600, 400}, {30, 200}, Qt::Vertical, "n=131 397:6 394:3 391:3 388:3 385:3 382:10:-0.6 379:3 376:3 373:3 370:3 367:6 364:3 361:3 357:3 354:3 351:10:-0.5 348:3 345:3 342:3 339:3 336:6 333:3 330:3 327:3 324:3 321:10:-0.4 318:3 315:3 312:3 309:3 306:6 303:3 300:3 297:3 293:3 290:10:-0.3 287:3 284:3 281:3 278:3 275:6 272:3 269:3 266:3 263:3 260:10:-0.2 257:3 254:3 251:3 248:3 245:6 242:3 239:3 236:3 233:3 229:10:-0.1 226:3 223:3 220:3 217:3 214:6 211:3 208:3 205:3 202:3 200:10:0:b 197:3 194:3 191:3 188:3 185:6 182:3 179:3 176:3 173:3 170:10:0.1 166:3 163:3 160:3 157:3 154:6 151:3 148:3 145:3 142:3 139:10:0.2 136:3 133:3 130:3 127:3 124:6 121:3 118:3 115:3 112:3 109:10:0.3 106:3 102:3 99:3 96:3 93:6 90:3 87:3 84:3 81:3 78:10:0.4 75:3 72:3 69:3 66:3 63:6 60:3 57:3 54:3 51:3 48:10:0.5 45:3 42:3 38:3 35:3 32:6 29:3 26:3 23:3 20:3 17:10:0.6 14:3 11:3 8:3 5:3 2:6"},
    RulerCase{377.95296, "in", {-30, 0, 300, 200}, {437, 301}, Qt::Vertical, "n=41 195:6 191:3 186:6 181:3 176:6 171:3 167:6 162:3 157:10:15 152:3 147:6 143:3 138:6 133:3 128:6 123:3 119:6 114:3 109:10:20:b 104:3 99:6 95:3 90:6 85:3 80:6 75:3 71:6 66:3 61:10:25 56:3 51:6 47:3 42:6 37:3 32:6 27:3 23:6 18:3 13:10:30:b 8:3 3:6"},
    RulerCase{37.795296, "in", {0, 0, 600, 400}, {30, 200}, Qt::Horizontal, "n=125 2:6 7:3 12:6 17:3 21:6 26:3 30:10:0:b 35:3 40:6 44:3 49:6 54:3 59:6 64:3 68:6 73:3 78:10:50 83:3 88:6 92:3 97:6 102:3 107:6 112:3 116:6 121:3 126:10:100:b 131:3 136:6 140:3 145:6 150:3 155:6 160:3 164:6 169:3 174:10:150 179:3 184:6 188:3 193:6 198:3 203:6 208:3 212:6 217:3 222:10:200:b 227:3 232:6 236:3 241:6 246:3 251:6 256:3 260:6 265:3 270:10:250 275:3 280:6 284:3 289:6 294:3 299:6 304:3 308:6 313:3 318:10:300:b 323:3 328:6 332:3 337:6 342:3 347:6 352:3 356:6 361:3 366:10:350 371:3 376:6 380:3 385:6 390:3 395:6 400:3 404:6 409:3 414:10:400:b 419:3 424:6 428:3 433:6 438:3 443:6 448:3 452:6 457:3 462:10:450 467:3 472:6 476:3 481:6 486:3 491:6 496:3 500:6 505:3 510:10:500:b 515:3 520:6 524:3 529:6 534:3 539:6 544:3 548:6 553:3 558:10:550 563:3 568:6 572:3 577:6 582:3 587:6 592:3 596:6"},
    RulerCase{12345.678, "mm", {150, 75, 600, 400}, {437, 301}, Qt::Vertical, "n=65 473:6 467:3 460:6 454:3 448:6 442:3 436:6 430:3 423:10:-10:b 417:3 411:6 405:3 399:6 393:3 386:6 380:3 374:6 368:3 362:10:-5 356:3 349:6 343:3 337:6 331:3 325:6 319:3 312:6 306:3 301:10:0:b 295:3 289:6 282:3 276:6 270:3 264:6 258:3 252:6 245:3 239:10:5 233:3 227:6 221:3 215:6 208:3 202:6 196:3 190:6 184:3 178:10:10:b 171:3 165:6 159:3 153:6 147:3 141:6 134:3 128:6 122:3 116:10:15 110:3 103:6 97:3 91:6 85:3 79:6"},
    RulerCase{12345.678, "ft", {150, 75, 600, 400}, {437, 301}, Qt::Horizontal, "n=160 152:3 156:6 160:3 163:3 167:3 171:3 175:10:-0.07 178:3 182:3 186:3 190:3 193:6 197:3 201:3 205:3 208:3 212:10:-0.06 216:3 220:3 224:3 227:3 231:6 235:3 239:3 242:3 246:3 250:10:-0.05 254:3 257:3 261:3 265:3 269:6 272:3 276:3 280:3 284:3 287:10:-0.04 291:3 295:3 299:3 303:3 306:6 310:3 314:3 318:3 321:3 325:10:-0.03 329:3 333:3 336:3 340:3 344:6 348:3 351:3 355:3 359:3 363:10:-0.02 367:3 370:3 374:3 378:3 382:6 385:3 389:3 393:3 397:3 400:10:-0.01 404:3 408:3 412:3 415:3 419:6 423:3 427:3 430:3 434:3 437:10:0:b 441:3 445:3 448:3 452:3 456:6 460:3 463:3 467:3 471:3 475:10:0.01 478:3 482:3 486:3 490:3 493:6 497:3 501:3 505:3 508:3 512:10:0.02 516:3 520:3 524:3 527:3 531:6 535:3 539:3 542:3 546:3 550:10:0.03 554:3 557:3 561:3 565:3 569:6 572:3 576:3 580:3 584:3 588:10:0.04 591:3 595:3 599:3 603:3 606:6 610:3 614:3 618:3 621:3 625:10:0.05 629:3 633:3 636:3 640:3 644:6 648:3 651:3 655:3 659:3 663:10:0.06 667:3 670:3 674:3 678:3 682:6 685:3 689:3 693:3 697:3 700:10:0.07 704:3 708:3 712:3 715:3 719:6 723:3 727:3 731:3 734:3 738:10:0.08 742:3 746:3 749:3"},
};
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

void expectRulerCase(const RulerCase& ruler)
{
    SCOPED_TRACE(std::to_string(ruler.scale) + " " + ruler.unit +
                 (ruler.orientation == Qt::Horizontal ? " H" : " V"));
    const bool horizontal = ruler.orientation == Qt::Horizontal;
    const int  start      = horizontal ? ruler.view.at(0) : ruler.view.at(1);
    const int  end        = start + (horizontal ? ruler.view.at(2) : ruler.view.at(3));
    EXPECT_EQ(describe(Ruler::marks(ruler.orientation, lengthUnit(ruler.unit), ruler.scale,
                                    ruler.origin, start, end)),
              ruler.expected);
}

TEST_F(RulerTest, TicksAndLabelsAsOpenRocket)
{
    for (const RulerCase& ruler : kRulerCases)
    {
        expectRulerCase(ruler);
    }
}

TEST_F(RulerTest, NoTicksForAScaleThatIsNotAPositiveFiniteNumber)
{
    const Unit& metre = lengthUnit("m");
    for (const double scale :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        EXPECT_TRUE(Ruler::marks(Qt::Horizontal, metre, scale, {30, 200}, 0, 600).empty()) << scale;
        EXPECT_TRUE(Ruler::marks(Qt::Vertical, metre, scale, {30, 200}, 0, 400).empty()) << scale;
    }
    // So small that the tick distances are infinite (Unit::getTicks() would fail).
    EXPECT_TRUE(Ruler::marks(Qt::Horizontal, metre, 1e-310, {30, 200}, 0, 600).empty());
    // So large that the values at the pixels are below the smallest double: still ticks.
    EXPECT_FALSE(Ruler::marks(Qt::Horizontal, metre, 1e300, {30, 200}, 0, 600).empty());
}

TEST_F(RulerTest, FollowsTheFigureInItsScrollArea)
{
    QWidget         holder;  // the figure's parent until the area takes it over
    auto*           figure = makeChild<TestScaleFigure>(&holder, kSubject, FigureView::SIDE);
    ScaleScrollArea area(figure, nullptr);
    area.resize(640, 480);
    area.show();
    area.setScaling(2.0);
    Ruler* horizontal = area.horizontalRuler();
    Ruler* vertical   = area.verticalRuler();
    EXPECT_EQ(horizontal->orientation(), Qt::Horizontal);
    EXPECT_EQ(vertical->orientation(), Qt::Vertical);

    area.horizontalScrollBar()->setValue(120);
    area.verticalScrollBar()->setValue(40);
    EXPECT_EQ(horizontal->visibleRange(), std::make_pair(120, 120 + area.viewport()->width()));
    EXPECT_EQ(vertical->visibleRange(), std::make_pair(40, 40 + area.viewport()->height()));
    EXPECT_EQ(describe(horizontal->marks()),
              describe(Ruler::marks(Qt::Horizontal, area.rulerUnit(), figure->absoluteScale(),
                                    figure->subjectOrigin(), 120, 120 + area.viewport()->width())));
    EXPECT_FALSE(horizontal->marks().empty());

    // The unit changes the labels.
    area.setRulerUnit(lengthUnit("mm"));
    EXPECT_EQ(&horizontal->unit(), &lengthUnit("mm"));
    EXPECT_EQ(&vertical->unit(), &lengthUnit("mm"));
    EXPECT_EQ(describe(vertical->marks()),
              describe(Ruler::marks(Qt::Vertical, lengthUnit("mm"), figure->absoluteScale(),
                                    figure->subjectOrigin(), 40, 40 + area.viewport()->height())));

    // Both paint.
    EXPECT_FALSE(horizontal->grab().toImage().isNull());
    EXPECT_FALSE(vertical->grab().toImage().isNull());
    area.hide();
}

TEST_F(RulerTest, WithoutAViewportTheRulerCoversTheFigure)
{
    TestScaleFigure figure(kSubject, FigureView::SIDE, nullptr);
    figure.resize(700, 500);
    const Ruler horizontal(Qt::Horizontal, &figure, nullptr);
    const Ruler vertical(Qt::Vertical, &figure, nullptr);
    EXPECT_EQ(horizontal.visibleRange(), std::make_pair(0, figure.width()));
    EXPECT_EQ(vertical.visibleRange(), std::make_pair(0, figure.height()));
    EXPECT_EQ(&horizontal.unit(), &unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
    EXPECT_EQ(horizontal.sizeHint(), QSize(figure.width() + 10, Ruler::kSize));
    EXPECT_EQ(vertical.sizeHint(), QSize(Ruler::kSize, figure.height() + 10));
}

}  // namespace

}  // namespace QtRocket::Gui
