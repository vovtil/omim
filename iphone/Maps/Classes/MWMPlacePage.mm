//
//  MWMPlacePageView.m
//  Maps
//
//  Created by v.mikhaylenko on 18.05.15.
//  Copyright (c) 2015 MapsWithMe. All rights reserved.
//

#import "MWMPlacePage.h"
#import "MWMBasePlacePageView.h"
#import "MWMPlacePageEntity.h"
#import "MWMPlacePageViewManager.h"
#import "MWMPlacePageActionBar.h"
#import "MWMDirectionView.h"
#import "MWMBookmarkColorViewController.h"
#import "SelectSetVC.h"
#import "UIKitCategories.h"
#import "MWMBookmarkDescriptionViewController.h"

#import "../../3party/Alohalytics/src/alohalytics_objc.h"

static NSString * const kPlacePageNibIdentifier = @"PlacePageView";
extern NSString * const kAlohalyticsTapEventKey;

@interface MWMPlacePage ()

@property (weak, nonatomic, readwrite) MWMPlacePageViewManager * manager;

@end

@implementation MWMPlacePage

- (instancetype)initWithManager:(MWMPlacePageViewManager *)manager
{
  self = [super init];
  if (self)
  {
    [[NSBundle mainBundle] loadNibNamed:kPlacePageNibIdentifier owner:self options:nil];
    self.manager = manager;
  }
  return self;
}

- (void)configure
{
  MWMPlacePageEntity * entity = self.manager.entity;
  [self.basePlacePageView configureWithEntity:entity];

  if (!self.actionBar)
    self.actionBar = [MWMPlacePageActionBar actionBarForPlacePage:self];

  MWMPlacePageEntityType type = entity.type;
  BOOL const isBookmark = type == MWMPlacePageEntityTypeBookmark;
  self.actionBar.bookmarkButton.selected = isBookmark;

  BOOL const isMyPosition = type == MWMPlacePageEntityTypeMyPosition;
  [self.actionBar configureForMyPosition:isMyPosition];
}

- (void)show
{
  // Should override this method if you want to show place page with custom animation.
}

- (void)dismiss
{
  [self.extendedPlacePageView removeFromSuperview];
  [self.actionBar removeFromSuperview];
  self.actionBar = nil;
}

#pragma mark - Actions

- (void)addBookmark
{
  [self.manager addBookmark];
  [self.basePlacePageView addBookmark];
}

- (void)removeBookmark
{
  [self.manager removeBookmark];
  [self.basePlacePageView removeBookmark];
}

- (void)share
{
  [Alohalytics logEvent:kAlohalyticsTapEventKey withValue:@"ppShare"];
  [self.manager share];
}

- (void)route
{
  [Alohalytics logEvent:kAlohalyticsTapEventKey withValue:@"ppRoute"];
  [self.manager buildRoute];
}

- (void)stopBuildingRoute
{
  self.actionBar.routeButton.hidden = NO;
  [self.actionBar dismissActivityIndicatior];
}

- (void)setDirectionArrowTransform:(CGAffineTransform)transform
{
  self.basePlacePageView.directionArrow.transform = transform;
}

- (void)setDistance:(NSString *)distance
{
  self.basePlacePageView.distanceLabel.text = distance;
}

- (void)updateMyPositionStatus:(NSString *)status
{
  [self.basePlacePageView updateAndLayoutMyPositionSpeedAndAltitude:status];
}

- (void)changeBookmarkCategory
{
  MWMPlacePageViewManager * manager = self.manager;
  SelectSetVC * vc = [[SelectSetVC alloc] initWithPlacePageManager:manager];
  [manager.ownerViewController.navigationController pushViewController:vc animated:YES];
}

- (void)changeBookmarkColor
{
  MWMBookmarkColorViewController * controller = [[MWMBookmarkColorViewController alloc] initWithNibName:[MWMBookmarkColorViewController className] bundle:nil];
  controller.placePageManager = self.manager;
  [self.manager.ownerViewController.navigationController pushViewController:controller animated:YES];
}

- (void)changeBookmarkDescription
{
  MWMBookmarkDescriptionViewController * viewController = [[MWMBookmarkDescriptionViewController alloc] initWithPlacePageManager:self.manager];
  [self.manager.ownerViewController.navigationController pushViewController:viewController animated:YES];
}

- (void)reloadBookmark
{
  [self.basePlacePageView reloadBookmarkCell];
}

- (void)willStartEditingBookmarkTitle:(CGFloat)keyboardHeight
{
// This method should be empty if your target is iPad.
}

- (void)willFinishEditingBookmarkTitle:(CGFloat)keyboardHeight
{
// This method should be empty if your target is iPad.
}

- (IBAction)didTap:(UITapGestureRecognizer *)sender
{
// This method should be ovverriden if you want to process custom tap.
}

- (IBAction)didPan:(UIPanGestureRecognizer *)sender
{
  // This method should be ovverriden if you want to process custom pan.
}

@end