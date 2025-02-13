/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { connect } from "react-redux";
import { TopSites as OldTopSites } from "content-src/components/TopSites/TopSites";
import React from "react";

export class _TopSites extends React.PureComponent {
  // Find a SPOC that doesn't already exist in User's TopSites
  getFirstAvailableSpoc(topSites, data) {
    const { spocs } = data;
    if (!spocs || spocs.length === 0) {
      return null;
    }

    const userTopSites = new Set(
      topSites.map(topSite => topSite && topSite.url)
    );

    // We "clean urls" with http in TopSiteForm.jsx
    // Spoc domains are in the format 'sponsorname.com'
    return spocs.find(
      spoc =>
        !userTopSites.has(spoc.url) &&
        !userTopSites.has(`http://${spoc.domain}`) &&
        !userTopSites.has(`https://${spoc.domain}`) &&
        !userTopSites.has(`http://www.${spoc.domain}`) &&
        !userTopSites.has(`https://www.${spoc.domain}`)
    );
  }

  // For the time being we only support 1 position.
  insertSpocContent(TopSites, data, promoPosition) {
    if (
      !TopSites.rows ||
      TopSites.rows.length === 0 ||
      !data.spocs ||
      data.spocs.length === 0
    ) {
      return null;
    }

    let topSites = [...TopSites.rows];
    const topSiteSpoc = this.getFirstAvailableSpoc(topSites, data);

    if (!topSiteSpoc) {
      return null;
    }

    const link = {
      customScreenshotURL: topSiteSpoc.image_src,
      type: "SPOC",
      label: topSiteSpoc.sponsor,
      title: topSiteSpoc.sponsor,
      url: topSiteSpoc.url,
      flightId: topSiteSpoc.flight_id,
      id: topSiteSpoc.id,
      guid: topSiteSpoc.id,
      shim: topSiteSpoc.shim,
      // For now we are assuming position based on intended position.
      // Actual position can shift based on other content.
      // We also hard code left and right to be 0 and 7.
      // We send the intended position in the ping.
      pos: promoPosition,
    };

    const replaceCount = topSites[promoPosition]?.show_sponsored_label ? 1 : 0;
    topSites.splice(promoPosition, replaceCount, link);

    return { ...TopSites, rows: topSites };
  }

  render() {
    const { header = {}, data, promoPositions, TopSites } = this.props;

    const TopSitesWithSpoc =
      TopSites && data && promoPositions?.length
        ? this.insertSpocContent(TopSites, data, promoPositions[0].index)
        : null;

    return (
      <div
        className={`ds-top-sites ${TopSitesWithSpoc ? "top-sites-spoc" : ""}`}
      >
        <OldTopSites
          isFixed={true}
          title={header.title}
          TopSitesWithSpoc={TopSitesWithSpoc}
        />
      </div>
    );
  }
}

export const TopSites = connect(state => ({ TopSites: state.TopSites }))(
  _TopSites
);
