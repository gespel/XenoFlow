%%% Create a UPPoster with LaTeX
%%% Based on a template by Franziska Schloesser, ZIB 20. May 2019
%%% 
%%% ------------------------------------------------------------------------------------------------
%%% LaTeX class file for UP poster.
%%% Hannes Signer, 11.02.2026
%%% ------------------------------------------------------------------------------------------------

\NeedsTeXFormat{LaTeX2e}[2018/04/01]
\ProvidesClass{upposter}

\RequirePackage{xcolor,graphicx}
\RequirePackage{calc}


\RequirePackage{upcolors}

%% pass on options (tikzposter ignores global options)
\DeclareOption{50pt}{
	\PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{40pt}{
	\PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{35pt}{
	\PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{30pt}{
	\PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{25pt}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{20pt}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{17pt}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{14pt}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{12pt}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{b1paper}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
  \PassOptionsToClass{innermargin=20mm}{tikzposter}
  \def\upposter@papersize{b1paper}
}
\DeclareOption{a0paper}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
  \PassOptionsToClass{innermargin=28mm}{tikzposter}
    \def\upposter@papersize{a0paper}
}

\DeclareOption{portrait}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}
\DeclareOption{landscape}{
  \PassOptionsToClass{\CurrentOption}{tikzposter}
}

\DeclareOption{draft}{
  \AtEndDocument{\guideline}
}

\DeclareOption{final}{
  \AtEndOfClass{\renewcommand{\guideline}{}}
}



\newif\ifupposter@pageborders\upposter@pagebordersfalse
\DeclareOption{pageborders}{\upposter@pageborderstrue}

% define color schemes

% 'colorscheme1': boxes in primary color (up colors)
\DeclareOption{colorscheme1}{
  \definecolor{upbg}{named}{white}
  \definecolor{upbgfg}{HTML}{d5d7d4}
  \definecolor{upfg}{named}{UPtextGray}
  \definecolor{uptitlefg}{named}{UPtextGray}
  \definecolor{upboxtitlebg}{named}{UPprimary}
  \definecolor{upboxtitle}{named}{white}
  \upposter@logodistantbackgroundtrue
}

% 'colorscheme2': boxes in secondary colors (math-nat colors)
\DeclareOption{colorscheme2}{
  \definecolor{upbg}{named}{white}
  \definecolor{upbgfg}{HTML}{d5d7d4}
  \definecolor{upfg}{named}{UPtextGray}
  \definecolor{uptitlefg}{named}{UPtextGray}
  \definecolor{upboxtitlebg}{named}{UPsecondary}
  \definecolor{upboxtitle}{named}{white}
  \upposter@logodistantbackgroundtrue
}

\newcommand{\uplogo}{graphics/logos/up_logo}
\newif\ifupposter@logodistantbackground
\upposter@logodistantbackgroundtrue

\DeclareOption{fontsans}{
  \renewcommand{\familydefault}{\sfdefault}
}

\DeclareOption{fontserif}{
  \renewcommand{\familydefault}{\rmdefault}
}

\DeclareOption{sansmath}{
  \AtEndOfPackage{\sansmath}
}


%% Execute default options
\ExecuteOptions{colorscheme1}
%\ExecuteOptions{fontsans}
\ExecuteOptions{25pt,b1paper,portrait}

%% Process given options
\ProcessOptions\relax

%% Load base
\LoadClass[margin=0mm, blockverticalspace=10mm, colspace=10mm, subcolspace=10mm]{tikzposter}

\RequirePackage{fontawesome}

\RequirePackage[sfdefault]{carlito}

% times new roman clone
\RequirePackage{mathptmx}

\RequirePackage{sansmath}



% HACK: tikzposter v2.0 doesn't support paper sizes it doesn't know
% like b1paper.  Allow at least setting an arbitrary paper size via
% \geometry{...} *after* this class has been loaded.
% Recompute documented internal variables
% \TP@visibletextheight, \TP@visibletextwidth
% without relying on undocumented ones (like \TP@innermargin).
\newlength\upposter@doublemargin
\setlength{\upposter@doublemargin}{%
  \textheight -\TP@visibletextheight}
% frames in up building require 700mm x 1000mm
% default b1paper format is 707mm x 1000mm 
\newlength\upposter@frameOffsetPageborders
\AtBeginDocument{%
  \setlength{\TP@visibletextheight}{%
    \textheight -\upposter@doublemargin}%
  \setlength{\TP@visibletextwidth}{%
    \textwidth -\upposter@doublemargin}}
  % set offset for dashed page bordes in portrait mode
  % fixme: handle offset in landscape mode, A1 paper format
  \ifdim\paperheight>\paperwidth
    \setlength{\upposter@frameOffsetPageborders}{3.5mm}
  \else
    \setlength{\upposter@frameOffsetPageborders}{0mm}
  \fi

\RequirePackage{geometry}
\geometry{\upposter@papersize}

\usetikzlibrary{intersections}

%% Add a strut to ensure uniform block title height.
\RequirePackage{etoolbox}
\let\upposter@orig@block\block
\newcommand{\upposter@block}[3][]{%
  \ifstrempty{#2}{\upposter@orig@block[{#1}]{}{#3}}%
  {\upposter@orig@block[{#1}]{{\normalsize\strut}#2}{#3}}}
\let\block\upposter@block

%% No heading for bibliography, small font
\let\upposter@orig@thebibliography\thebibliography
\newcommand{\upposter@thebibliography}{\def\section##1##{\@gobble}%
  \small\upposter@orig@thebibliography}
\let\thebibliography\upposter@thebibliography

%% Don't scale title
% Handle (unlikely) # in argument of \title properly.
\renewcommand{\title}[1]{\edef\@title{\unexpanded{#1}}}

%% Additional packages and commands.
\RequirePackage{enumitem}
\AtBeginDocument{%
  \ifupposter@pageborders
    \draw[black, line width=1pt, dashed]
    ([xshift=\upposter@frameOffsetPageborders]bottomleft) rectangle ([xshift=-\upposter@frameOffsetPageborders]topright);
  \fi
}

\AtBeginDocument{%
  \path (bottomleft |- {(0, -\TP@visibletextheight/2)})
    coordinate (upposter@textbottomleft);}

\newcommand{\guideline}{%
    \draw[red, line width=1mm] (upposter@textbottomleft)
    -- (topright |- upposter@textbottomleft);%
}
\newcommand{\upposter@titlelogos}{}
\newcommand{\titlelogos}{\forcsvlist{\listadd\upposter@titlelogos}}
\newcommand{\upposter@logos}{\ignorespaces\forlistloop{\space%
    \includegraphics[height=3.17cm]}{\upposter@titlelogos}}

% Redefine itemize environment
\setlist[itemize]{leftmargin=1.5em,itemsep=0pt,topsep=0pt,partopsep=0pt, parsep=0pt}
\renewcommand\labelitemi{\faCaretRight\hspace{.8em}}

% assign named colors required by tikzposter
\definecolorstyle{upstyle}{
}{
  % Background Colors
  \colorlet{backgroundcolor}{upbg}
  \colorlet{framecolor}{upfg}
  % Title Colors
  \colorlet{titlefgcolor}{uptitlefg}
  % Title background color is currently ignored.
  \colorlet{titlebgcolor}{-uptitlefg}
  % Block Colors
  \colorlet{blocktitlebgcolor}{upboxtitlebg}
  \colorlet{blocktitlefgcolor}{upboxtitle}
  \colorlet{blockbodybgcolor}{white}
  \colorlet{blockbodyfgcolor}{black}
  % Innerblock Colors
  \colorlet{innerblocktitlebgcolor}{white}
  \colorlet{innerblocktitlefgcolor}{black}
  \colorlet{innerblockbodybgcolor}{white}
  \colorlet{innerblockbodyfgcolor}{black}
  % Note colors
  \colorlet{notefgcolor}{black}
  \colorlet{notebgcolor}{white}
  \colorlet{noteframecolor}{upboxtitlebg}
}

\tikzposterlatexaffectionproofoff %shows small comment on how the poster was made at bottom of poster

\newlength{\upposter@tmpwidth}
\newlength{\upposter@tmpheight}
\definetitlestyle{upposter}{%
	width=\ifdim\textwidth>\textheight.955\else.94\fi\textwidth,
	roundedcorners=0,
	linewidth=0pt, innersep=0cm,
	titletotopverticalspace=\ifdim\textwidth>\textheight
	.025\else.035\fi\textwidth,
	titletoblockverticalspace=\ifdim\textwidth>\textheight
	.02\else.033\fi\textwidth}{%
	\@ifundefined{backgroundgraphic}{}{}%
	\fill[color=black!10] (-0.5\paperwidth, 0.5\paperheight) rectangle (0.5\paperwidth, 0.99\titleposbottom);
	\draw[color=framecolor, line width=2mm] (-0.5\paperwidth, 0.99\titleposbottom) -- (0.5\paperwidth, 0.99\titleposbottom);
}

% Draw curve of up logo of color #2; no margins, with bottom edge on
% baseline.  #1 is additional drawing commands like clipping
%\newcommand{\upposter@curve}[2][]{%
%  \begin{tikzpicture}[x=10pt, y=10pt]
%  #1
%  \path[fill={#2}]
%    (107.624, 118.23211)
%    -- (121.002, 118.23211)
%    .. controls (119.314, 52.86991) and (65.716, 0.19805)
%    .. (0, 0)
%    -- (0, 13.36911)
%    .. controls (58.344, 13.56721) and (105.94, 60.24301)
%    .. cycle;
%  \end{tikzpicture}}

% Draw curve of color #3 clipped to fit into #1 wide and #2 rectangle.
% TODO: This is noticably a bit slow.
%\newcommand{\upposter@curveclipped}[3]{\upposter@curve[{%
%  \begin{pgfinterruptboundingbox}
%    \path[name path=lower curve] (121.002, 118.23211) .. controls
%    (119.314, 52.86991) and (65.716, 0.19805) .. (0, 0);
%    % The diagonal of the area curve should fill
%    \path[name path=diagonal] (121.002, 118.23211) --
%    ++(xyz cs:x=-{#2/#3}*200, y=-200);
%    \tikzset{name intersections={of=lower curve and diagonal,
%        sort by=diagonal,
%        total=\upposter@tmp}}
%    \global\let\upposter@tmp\upposter@tmp
%  \end{pgfinterruptboundingbox}
%  \ifnum\upposter@tmp>1
%    \clip (intersection-2) rectangle (121.002, 118.23211);
%    \fi}]{#1}}

\newsavebox{\upposter@tmpbox}
\newlength{\upposter@titlewidth}
% Minimum distance between title and up logo
\providecommand{\upposterlogosep}{1cm}

\settitle{
  \begin{minipage}{\linewidth}
    \Large
    \color{titlefgcolor}
    \savebox{\upposter@tmpbox}{%
      \includegraphics[width=12cm]{\uplogo}}
    \setlength{\upposter@titlewidth}{\linewidth
      - \wd\upposter@tmpbox
      - \upposterlogosep}
    \hbox{%
      \parbox[b]{\upposter@titlewidth}{
        \sffamily
        \raggedright \bfseries
        % Is there a better way selecting a relative font size?
        % \Huge is too small.
        % The factors are chosen to produce 88pt font size and 105pt
        % baselinsekip with the default document font size.
        \normalsize
        \fontsize{\dimexpr \f@size\p@ * 1100/311\relax}{\dimexpr
          \f@baselineskip * 7/2\relax}
        \selectfont
        \penalty1 %
        \textcolor{UPprimary}{\@title}
      }%
      % FIXME: There should be a cleaner way to put \parbox into a box
      % register.
      \setbox0\lastbox
      % Test on available space is somewhat arbitrary.
      \ifdim \dimexpr \ht0 + 2\baselineskip \relax
             > \ht\upposter@tmpbox\relax
        \parbox[t]{\upposter@titlewidth}{\unvbox0}
        % Little space available besides logo, move further title
        % elements below the logo.
        \aftergroup\upposter@authorship
      \else
        \parbox[t]{\upposter@titlewidth}{%
          \prevdepth\dp0 \unvbox0 %
          % Don't leave large space besides logo, fill it with
          % following title elements.
          \upposter@authorship
     
        }
      \fi
      \parbox[t]{\wd\upposter@tmpbox}{\penalty1 %
      	\raisebox{-10cm}[0.5cm][1cm]{\usebox{\upposter@tmpbox}}% 
      }  
   }%
%   \par
%   \vspace{1em}
%   {\color{framecolor}\hrule height 2mm}
 \end{minipage}}

\newcommand{\upposter@authorship}{%
	\@author
	\par
	\vspace*{.4em}
	\sbox{\upposter@tmpbox}{\upposter@logos}
	\mbox{%
		\parbox{\linewidth - \wd\upposter@tmpbox}{%
			\@institute
		}%
		\parbox{\wd\upposter@tmpbox}{%
			\ifupposter@logodistantbackground
			\usebox{\upposter@tmpbox}%
			\else
			\colorbox{white}{\usebox{\upposter@tmpbox}}%
			\fi
		}%
	}%
}

\definebackgroundstyle{upposter}{%
  \path[fill=backgroundcolor] (bottomleft) rectangle (topright);
  \@ifundefined{backgroundgraphic}{}{%
    \node[above right, inner sep=0pt] at (bottomleft) {\includegraphics[height=\paperheight,
      width=\paperwidth]{\backgroundgraphic}};
}}

\defineblockstyle{upposter}{
    titlewidthscale=1, bodywidthscale=1, titlecenter, titleoffsetx=0pt, titleoffsety=0pt, bodyoffsetx=0pt, bodyoffsety=0pt,
    bodyverticalshift=0pt, roundedcorners=0, linewidth=1pt, titleinnersep=4.7mm, bodyinnersep=9mm
  }{
  \begin{scope}[line width=\blocklinewidth, rounded corners=\blockroundedcorners]
    \ifBlockHasTitle %
       \draw[color=framecolor,
        fill=blocktitlebgcolor]
       (blocktitle.south west) rectangle (blocktitle.north east);
    \fi
    \draw[color=framecolor, fill=blockbodybgcolor] (blockbody.south west) rectangle (blockbody.north east);
  \end{scope}
}

% -- PREDEFINED THEMES ---------------------- %
% Choose LAYOUT for \usetheme:  Default, Basic, Rays, Simple, Envelope, Wave, Board, Autumn, Desert,
\usetheme{Default}
\usetitlestyle{upposter}
\useblockstyle{upposter}
\usebackgroundstyle{upposter}
\useinnerblockstyle{TornOut}
\usecolorstyle{upstyle}

% global space between boxes
\setlength{\TP@blockverticalspace}{10mm}
\setlength{\TP@colspace}{10mm}

% needs to be the last command
\endinput
